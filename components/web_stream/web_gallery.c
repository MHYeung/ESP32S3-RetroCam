#include "web_stream.h"
#include "storage_sd.h"
#include "cJSON.h"
#include "esp_check.h"
#include "esp_crc.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "web_gallery";

#define DCIM_PATH      "/sdcard/DCIM"
#define MAX_NAME       64
#define MAX_PHOTOS     96
#define MAX_ZIP_FILES  48
#define MAX_POST_BODY  4096
#define MAX_PATH       (sizeof(DCIM_PATH) + MAX_NAME)

typedef struct {
    char name[MAX_NAME];
    time_t mtime;
    long size;
} photo_entry_t;

typedef struct {
    char name[MAX_NAME];
    uint32_t offset;
    uint32_t size;
    uint32_t crc;
    uint16_t mod_time;
    uint16_t mod_date;
} zip_entry_t;

static bool name_safe(const char *name)
{
    if (!name || name[0] == '\0') {
        return false;
    }
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) {
        return false;
    }
    return true;
}

static bool dcim_path_join(const char *name, char *path, size_t path_len)
{
    if (strlen(name) >= MAX_NAME) {
        return false;
    }
    if (strlcpy(path, DCIM_PATH, path_len) >= path_len) {
        return false;
    }
    if (strlcat(path, "/", path_len) >= path_len) {
        return false;
    }
    return strlcat(path, name, path_len) < path_len;
}

static int cmp_photo_desc(const void *a, const void *b)
{
    const photo_entry_t *pa = a;
    const photo_entry_t *pb = b;
    if (pa->mtime > pb->mtime) {
        return -1;
    }
    if (pa->mtime < pb->mtime) {
        return 1;
    }
    return 0;
}

static int collect_photos(photo_entry_t *out, int max_n)
{
    int n = 0;
    DIR *d = opendir(DCIM_PATH);
    if (!d) {
        return 0;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n < max_n) {
        const char *fn = ent->d_name;
        size_t len = strlen(fn);
        if (len < 5 || len >= MAX_NAME || strcasecmp(fn + len - 4, ".jpg") != 0) {
            continue;
        }
        char path[MAX_PATH];
        char name[MAX_NAME];
        strlcpy(name, fn, sizeof(name));
        if (!dcim_path_join(name, path, sizeof(path))) {
            continue;
        }
        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }
        strlcpy(out[n].name, name, sizeof(out[n].name));
        out[n].mtime = st.st_mtime;
        out[n].size = (long)st.st_size;
        n++;
    }
    closedir(d);
    if (n > 1) {
        qsort(out, (size_t)n, sizeof(photo_entry_t), cmp_photo_desc);
    }
    return n;
}

static uint16_t zip_dos_time(time_t t)
{
    struct tm tm;
    if (!localtime_r(&t, &tm)) {
        return 0;
    }
    return (uint16_t)(((tm.tm_hour & 0x1f) << 11) | ((tm.tm_min & 0x3f) << 5) | ((tm.tm_sec / 2) & 0x1f));
}

static uint16_t zip_dos_date(time_t t)
{
    struct tm tm;
    if (!localtime_r(&t, &tm)) {
        return 0;
    }
    return (uint16_t)((((tm.tm_year - 80) & 0x7f) << 9) | (((tm.tm_mon + 1) & 0xf) << 5) | (tm.tm_mday & 0x1f));
}

static esp_err_t http_send_bytes(httpd_req_t *req, const void *data, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }
    return httpd_resp_send_chunk(req, (const char *)data, (ssize_t)len);
}

static esp_err_t zip_write_local(httpd_req_t *req, const zip_entry_t *e, uint32_t *offset)
{
    const uint16_t nlen = (uint16_t)strlen(e->name);
    uint8_t hdr[30];

    hdr[0] = 0x50;
    hdr[1] = 0x4b;
    hdr[2] = 0x03;
    hdr[3] = 0x04;
    hdr[4] = 10;
    hdr[5] = 0;
    hdr[6] = 0;
    hdr[7] = 0;
    hdr[8] = (uint8_t)(e->mod_time & 0xff);
    hdr[9] = (uint8_t)(e->mod_time >> 8);
    hdr[10] = (uint8_t)(e->mod_date & 0xff);
    hdr[11] = (uint8_t)(e->mod_date >> 8);
    memcpy(&hdr[12], &e->crc, 4);
    memcpy(&hdr[16], &e->size, 4);
    memcpy(&hdr[20], &e->size, 4);
    hdr[24] = (uint8_t)(nlen & 0xff);
    hdr[25] = (uint8_t)(nlen >> 8);
    hdr[26] = 0;
    hdr[27] = 0;

    ESP_RETURN_ON_ERROR(http_send_bytes(req, hdr, 30), TAG, "lh");
    ESP_RETURN_ON_ERROR(http_send_bytes(req, e->name, nlen), TAG, "ln");
    *offset += 30 + nlen;
    return ESP_OK;
}

static esp_err_t zip_write_central(httpd_req_t *req, const zip_entry_t *e)
{
    const uint16_t nlen = (uint16_t)strlen(e->name);
    uint8_t hdr[46];

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 0x50;
    hdr[1] = 0x4b;
    hdr[2] = 0x01;
    hdr[3] = 0x02;
    hdr[4] = 0x14;
    hdr[5] = 0;
    hdr[6] = 10;
    hdr[7] = 0;
    hdr[8] = 0;
    hdr[9] = 0;
    hdr[10] = (uint8_t)(e->mod_time & 0xff);
    hdr[11] = (uint8_t)(e->mod_time >> 8);
    hdr[12] = (uint8_t)(e->mod_date & 0xff);
    hdr[13] = (uint8_t)(e->mod_date >> 8);
    memcpy(&hdr[16], &e->crc, 4);
    memcpy(&hdr[20], &e->size, 4);
    memcpy(&hdr[24], &e->size, 4);
    hdr[28] = (uint8_t)(nlen & 0xff);
    hdr[29] = (uint8_t)(nlen >> 8);
    memcpy(&hdr[42], &e->offset, 4);

    ESP_RETURN_ON_ERROR(http_send_bytes(req, hdr, 46), TAG, "cd");
    return http_send_bytes(req, e->name, nlen);
}

static esp_err_t gallery_api_photos(httpd_req_t *req)
{
    photo_entry_t list[MAX_PHOTOS];
    storage_sd_lock();
    int n = collect_photos(list, MAX_PHOTOS);
    storage_sd_unlock();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"photos\":[");
    for (int i = 0; i < n; i++) {
        char item[160];
        snprintf(item, sizeof(item),
                 "%s{\"name\":\"%s\",\"mtime\":%ld,\"size\":%ld}",
                 i ? "," : "",
                 list[i].name,
                 (long)list[i].mtime,
                 list[i].size);
        httpd_resp_sendstr_chunk(req, item);
    }
    httpd_resp_sendstr_chunk(req, "]}");
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t gallery_download_zip(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > MAX_POST_BODY) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
        return ESP_FAIL;
    }

    char *body = malloc((size_t)req->content_len + 1);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
        return ESP_FAIL;
    }
    int got = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, body + got, (size_t)req->content_len - (size_t)got);
        if (r <= 0) {
            free(body);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "recv");
            return ESP_FAIL;
        }
        got += r;
    }
    body[got] = '\0';

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "json");
        return ESP_FAIL;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "names");
    if (!cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "names");
        return ESP_FAIL;
    }

    zip_entry_t entries[MAX_ZIP_FILES];
    int n = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, arr)
    {
        if (!cJSON_IsString(item) || !name_safe(item->valuestring)) {
            continue;
        }
        if (n >= MAX_ZIP_FILES) {
            break;
        }
        strlcpy(entries[n].name, item->valuestring, sizeof(entries[n].name));
        n++;
    }
    cJSON_Delete(root);

    if (n == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/zip");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"photos.zip\"");

    uint32_t zip_size = 0;
    uint32_t cd_start = 0;
    esp_err_t err = ESP_OK;

    storage_sd_lock();
    for (int i = 0; i < n && err == ESP_OK; i++) {
        char path[MAX_PATH];
        if (!dcim_path_join(entries[i].name, path, sizeof(path))) {
            err = ESP_FAIL;
            break;
        }
        struct stat st;
        if (stat(path, &st) != 0 || st.st_size <= 0) {
            err = ESP_FAIL;
            break;
        }
        entries[i].size = (uint32_t)st.st_size;
        entries[i].mod_time = zip_dos_time(st.st_mtime);
        entries[i].mod_date = zip_dos_date(st.st_mtime);
        entries[i].offset = zip_size;
        entries[i].crc = 0xFFFFFFFF;

        FILE *f = fopen(path, "rb");
        if (!f) {
            err = ESP_FAIL;
            break;
        }
        uint8_t buf[1024];
        size_t rd;
        while ((rd = fread(buf, 1, sizeof(buf), f)) > 0) {
            entries[i].crc = esp_crc32_le(entries[i].crc, buf, (uint32_t)rd);
        }
        fclose(f);
        entries[i].crc ^= 0xFFFFFFFF;

        err = zip_write_local(req, &entries[i], &zip_size);
        if (err != ESP_OK) {
            break;
        }

        f = fopen(path, "rb");
        if (!f) {
            err = ESP_FAIL;
            break;
        }
        while ((rd = fread(buf, 1, sizeof(buf), f)) > 0) {
            if (http_send_bytes(req, buf, rd) != ESP_OK) {
                err = ESP_FAIL;
                break;
            }
            zip_size += (uint32_t)rd;
        }
        fclose(f);
    }
    storage_sd_unlock();

    if (err != ESP_OK) {
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_FAIL;
    }

    cd_start = zip_size;
    for (int i = 0; i < n; i++) {
        if (zip_write_central(req, &entries[i]) != ESP_OK) {
            return ESP_FAIL;
        }
        zip_size += 46 + (uint32_t)strlen(entries[i].name);
    }

    uint32_t cd_size = zip_size - cd_start;
    uint8_t eocd[22];
    memset(eocd, 0, sizeof(eocd));
    eocd[0] = 0x50;
    eocd[1] = 0x4b;
    eocd[2] = 0x05;
    eocd[3] = 0x06;
    uint16_t cnt = (uint16_t)n;
    eocd[8] = (uint8_t)(cnt & 0xff);
    eocd[9] = (uint8_t)(cnt >> 8);
    eocd[10] = eocd[8];
    eocd[11] = eocd[9];
    memcpy(&eocd[12], &cd_size, 4);
    memcpy(&eocd[16], &cd_start, 4);
    http_send_bytes(req, eocd, sizeof(eocd));
    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "zip sent %d files (%lu bytes)", n, (unsigned long)(zip_size + 22));
    return ESP_OK;
}

static esp_err_t gallery_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    static const char page[] =
        "<!DOCTYPE html><html><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>ESP32-S3 DCIM</title><style>"
        "body{font-family:system-ui,sans-serif;margin:0;background:#0f0f12;color:#eee}"
        "header{display:flex;flex-wrap:wrap;align-items:center;gap:8px;padding:12px 16px;"
        "background:#1a1a22;position:sticky;top:0;z-index:2}"
        "h1{margin:0;font-size:1.1rem;flex:1}"
        "button{background:#3d5afe;color:#fff;border:0;padding:8px 14px;border-radius:6px;cursor:pointer}"
        "button.sec{background:#333}button:disabled{opacity:.5}"
        "main{padding:12px 16px 80px}"
        "section{margin-bottom:24px}"
        "section h2{font-size:.95rem;color:#9ab;margin:0 0 10px;border-bottom:1px solid #333;padding-bottom:6px}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}"
        ".card{background:#1a1a22;border-radius:8px;overflow:hidden;border:1px solid #2a2a33}"
        ".thumb{border:0;padding:0;margin:0;width:100%;cursor:pointer;background:#000}"
        ".thumb img{width:100%;aspect-ratio:4/3;object-fit:cover;display:block}"
        ".card label{display:flex;align-items:center;gap:6px;padding:6px 8px;font-size:.75rem;color:#aaa;cursor:pointer}"
        ".card input{accent-color:#3d5afe}"
        "#status{padding:8px 16px;color:#8f8;font-size:.85rem}"
        "#viewer{position:fixed;inset:0;background:rgba(0,0,0,.92);z-index:10;display:none;flex-direction:column}"
        "#viewer.open{display:flex}"
        ".vbar{display:flex;align-items:center;gap:8px;padding:12px 16px;background:#1a1a22;"
        "border-bottom:1px solid #333}"
        ".vbar h2{flex:1;margin:0;font-size:1rem;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".vframe{flex:1;display:flex;align-items:center;justify-content:center;padding:16px}"
        ".vframe img{max-width:100%;max-height:100%;object-fit:contain;border:8px solid #222;border-radius:4px;"
        "box-shadow:0 8px 32px rgba(0,0,0,.5);background:#000}"
        "</style></head><body>"
        "<header><h1>DCIM Gallery</h1>"
        "<button type=button class=sec id=selAll>Select all</button>"
        "<button type=button id=dl>Download</button></header>"
        "<div id=status>Loading...</div><main id=main></main>"
        "<div id=viewer><div class=vbar><h2 id=vtitle></h2>"
        "<button type=button class=sec id=vclose>Close</button></div>"
        "<div class=vframe><img id=vimg alt=''></div></div>"
        "<script>"
        "const GAP=4*3600,main=document.getElementById('main'),st=document.getElementById('status');"
        "const viewer=document.getElementById('viewer'),vtitle=document.getElementById('vtitle'),"
        "vimg=document.getElementById('vimg');"
        "function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/\"/g,'&quot;');}"
        "function openView(name){"
        "vtitle.textContent=name;vimg.src='/photo?name='+encodeURIComponent(name);viewer.classList.add('open');}"
        "document.getElementById('vclose').onclick=()=>viewer.classList.remove('open');"
        "viewer.onclick=e=>{if(e.target===viewer)viewer.classList.remove('open');};"
        "function sectionTitle(grp,idx,tNewest){"
        "const t0=grp[0].mtime,t1=grp[grp.length-1].mtime;"
        "const h0=((tNewest-t0)/3600).toFixed(1),h1=((tNewest-t1)/3600).toFixed(1);"
        "return 'Session '+(idx+1)+' ('+h1+' – '+h0+' h ago, '+grp.length+' photos)';}"
        "function buildSections(photos){"
        "const s=[];let g=[];"
        "photos.forEach(p=>{"
        "if(g.length&&(g[g.length-1].mtime-p.mtime)>GAP){s.push(g);g=[];} g.push(p);});"
        "if(g.length)s.push(g);return s;}"
        "fetch('/api/photos').then(r=>r.json()).then(d=>{"
        "const photos=d.photos||[];if(!photos.length){st.textContent='No photos.';return;}"
        "st.textContent=photos.length+' photo(s)';"
        "const tNew=photos[0].mtime;"
        "buildSections(photos).forEach((grp,i)=>{"
        "const sec=document.createElement('section');"
        "sec.innerHTML='<h2>'+esc(sectionTitle(grp,i,tNew))+'</h2><div class=grid></div>';"
        "const grid=sec.querySelector('.grid');"
        "grp.forEach(p=>{"
        "const card=document.createElement('div');card.className='card';"
        "const btn=document.createElement('button');btn.className='thumb';btn.type='button';"
        "const im=document.createElement('img');im.loading='lazy';im.src='/photo?name='+encodeURIComponent(p.name);"
        "btn.appendChild(im);btn.onclick=()=>openView(p.name);"
        "const lab=document.createElement('label');"
        "const cb=document.createElement('input');cb.type='checkbox';cb.dataset.name=p.name;"
        "lab.append(cb,document.createTextNode(p.name+' ('+Math.round(p.size/1024)+' KB)'));"
        "card.append(btn,lab);grid.appendChild(card);});"
        "main.appendChild(sec);});"
        "}).catch(e=>{st.textContent='Error: '+e;});"
        "document.getElementById('selAll').onclick=()=>{"
        "const c=[...document.querySelectorAll('input[type=checkbox]')];"
        "const all=c.length&&c.every(x=>x.checked);c.forEach(x=>x.checked=!all);};"
        "document.getElementById('dl').onclick=async()=>{"
        "const names=[...document.querySelectorAll('input[type=checkbox]:checked')]"
        ".map(x=>x.dataset.name);"
        "if(!names.length){alert('Select photos');return;}"
        "const btn=document.getElementById('dl');btn.disabled=true;"
        "if(names.length===1){"
        "const n=names[0];"
        "const a=document.createElement('a');"
        "a.href='/photo?name='+encodeURIComponent(n);a.download=n;"
        "document.body.appendChild(a);a.click();a.remove();"
        "st.textContent='Downloaded '+n;btn.disabled=false;return;}"
        "st.textContent='Preparing zip...';"
        "try{"
        "const r=await fetch('/api/download.zip',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({names})});"
        "if(!r.ok)throw new Error('zip failed');"
        "const blob=await r.blob();"
        "const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='photos.zip';"
        "a.click();URL.revokeObjectURL(a.href);st.textContent='Downloaded '+names.length+' photos (zip)';"
        "}catch(e){st.textContent='Zip error: '+e;}"
        "btn.disabled=false;};"
        "</script></body></html>";
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t gallery_photo(httpd_req_t *req)
{
    char name[MAX_NAME] = {0};
    if (httpd_req_get_url_query_len(req) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing name");
        return ESP_FAIL;
    }

    char qbuf[MAX_NAME + 8];
    if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad query");
        return ESP_FAIL;
    }
    if (httpd_query_key_value(qbuf, "name", name, sizeof(name)) != ESP_OK || !name_safe(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad name");
        return ESP_FAIL;
    }

    char path[MAX_PATH];
    if (!dcim_path_join(name, path, sizeof(path))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad name");
        return ESP_FAIL;
    }

    storage_sd_lock();
    FILE *f = fopen(path, "rb");
    if (!f) {
        storage_sd_unlock();
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    char buf[1024];
    size_t n;
    esp_err_t ret = ESP_OK;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, (ssize_t)n) != ESP_OK) {
            ret = ESP_FAIL;
            break;
        }
    }
    fclose(f);
    storage_sd_unlock();

    httpd_resp_send_chunk(req, NULL, 0);
    return ret;
}

esp_err_t web_gallery_start(httpd_handle_t *out_server)
{
    if (out_server && *out_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = 80;
    cfg.stack_size       = 16384;
    cfg.lru_purge_enable = true;

    httpd_handle_t srv = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&srv, &cfg), TAG, "httpd_start");

    httpd_uri_t u_root = { .uri = "/",               .method = HTTP_GET,  .handler = gallery_root        };
    httpd_uri_t u_api  = { .uri = "/api/photos",       .method = HTTP_GET,  .handler = gallery_api_photos  };
    httpd_uri_t u_zip  = { .uri = "/api/download.zip", .method = HTTP_POST, .handler = gallery_download_zip };
    httpd_uri_t u_photo = { .uri = "/photo",           .method = HTTP_GET,  .handler = gallery_photo     };
    httpd_register_uri_handler(srv, &u_root);
    httpd_register_uri_handler(srv, &u_api);
    httpd_register_uri_handler(srv, &u_zip);
    httpd_register_uri_handler(srv, &u_photo);

    ESP_LOGI(TAG, "gallery HTTP started");
    if (out_server) {
        *out_server = srv;
    }
    return ESP_OK;
}

void web_gallery_stop(httpd_handle_t *server)
{
    if (!server || !*server) {
        return;
    }
    httpd_stop(*server);
    *server = NULL;
    ESP_LOGI(TAG, "gallery HTTP stopped");
}
