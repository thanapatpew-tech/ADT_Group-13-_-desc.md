#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <time.h>       // <-- เพิ่มไลบรารี time.h สำหรับฟังก์ชัน clock()

// --- เพิ่ม 3 บรรทัดนี้สำหรับรองรับ Windows ---
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define APP_NAME "main.exe"
#define MAX_DORMS 64
#define NAME_LEN 128
#define URL_LEN 256
#define PER_PAGE 40     // จำนวนหอพักต่อหน้า
#define REPEAT 1000     // <-- เพิ่มจำนวนรอบสำหรับลูปจับเวลาหาค่าเฉลี่ย

typedef struct {
    int id;
    char name[NAME_LEN];
    int price;
    float distance;
    int facilities_score;
    char facilities_list[256];
    char image_url[URL_LEN];
} Dorm;

typedef struct {
    double score;
    Dorm dorm;
} HeapNode;

typedef struct {
    double w_price;
    double w_distance;
    double w_facility;
} Weights;

typedef struct {
    HeapNode data[MAX_DORMS];
    int size;
} MaxHeap;

static void get_param_str(const char *qs, const char *key, char *out_val, size_t max_len, const char *default_val) {
    if (!qs || strlen(qs) == 0) {
        strncpy(out_val, default_val, max_len - 1);
        out_val[max_len - 1] = '\0';
        return;
    }
    char key_buf[32];
    snprintf(key_buf, sizeof(key_buf), "%s=", key);
    char *p = strstr(qs, key_buf);
    if (p) {
        p += strlen(key_buf);
        size_t idx = 0;
        while (*p && *p != '&' && idx < max_len - 1) {
            out_val[idx++] = *p++;
        }
        out_val[idx] = '\0';
    } else {
        strncpy(out_val, default_val, max_len - 1);
        out_val[max_len - 1] = '\0';
    }
}

static double get_param_double(const char *qs, const char *key, double default_val) {
    char buf[32];
    get_param_str(qs, key, buf, sizeof(buf), "");
    if (strlen(buf) > 0) return atof(buf);
    return default_val;
}

static void heap_swap(HeapNode *a, HeapNode *b) {
    HeapNode tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sift_down(MaxHeap *h, int i) {
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < h->size && h->data[left].score > h->data[largest].score) largest = left;
    if (right < h->size && h->data[right].score > h->data[largest].score) largest = right;

    if (largest != i) {
        heap_swap(&h->data[i], &h->data[largest]);
        sift_down(h, largest);
    }
}

static void build_heap(MaxHeap *h) {
    for (int i = (h->size / 2) - 1; i >= 0; i--) {
        sift_down(h, i);
    }
}

static HeapNode heap_pop(MaxHeap *h) {
    HeapNode top = h->data[0];
    h->size--;
    if (h->size > 0) {
        h->data[0] = h->data[h->size];
        sift_down(h, 0);
    }
    return top;
}

static void compute_scores(Dorm *dorms, int n, Weights w, MaxHeap *heap) {
    int min_price = INT_MAX, max_price = INT_MIN;
    float min_dist = FLT_MAX, max_dist = -FLT_MAX;
    int min_fac = INT_MAX, max_fac = INT_MIN;

    for (int i = 0; i < n; i++) {
        if (dorms[i].price < min_price) min_price = dorms[i].price;
        if (dorms[i].price > max_price) max_price = dorms[i].price;
        if (dorms[i].distance < min_dist) min_dist = dorms[i].distance;
        if (dorms[i].distance > max_dist) max_dist = dorms[i].distance;
        if (dorms[i].facilities_score < min_fac) min_fac = dorms[i].facilities_score;
        if (dorms[i].facilities_score > max_fac) max_fac = dorms[i].facilities_score;
    }

    heap->size = 0;

    for (int i = 0; i < n; i++) {
        double price_range = (max_price - min_price) == 0 ? 1 : (max_price - min_price);
        double dist_range = (max_dist - min_dist) == 0 ? 1 : (max_dist - min_dist);
        double fac_range = (max_fac - min_fac) == 0 ? 1 : (max_fac - min_fac);

        double norm_price = 1.0 - ((dorms[i].price - min_price) / price_range);
        double norm_dist = 1.0 - ((dorms[i].distance - min_dist) / dist_range);
        double norm_fac = (dorms[i].facilities_score - min_fac) / fac_range;

        double score = w.w_price * norm_price + w.w_distance * norm_dist + w.w_facility * norm_fac;

        HeapNode node;
        node.score = score;
        node.dorm = dorms[i];
        heap->data[heap->size] = node;
        heap->size++;
    }
    build_heap(heap);
}

// ปุ่มเปลี่ยนหน้า (Pagination)
static void render_pagination(int page_num, int total_pages, Weights w) {
    printf("<div class='pagination'>");
    if (page_num > 1) {
        printf("<a class='page-btn' href='%s?page=algorithm&w_price=%.2f&w_dist=%.2f&w_fac=%.2f&pg=%d'>&larr; ก่อนหน้า</a>",
               APP_NAME, w.w_price, w.w_distance, w.w_facility, page_num - 1);
    }
    for (int i = 1; i <= total_pages; i++) {
        printf("<a class='page-btn%s' href='%s?page=algorithm&w_price=%.2f&w_dist=%.2f&w_fac=%.2f&pg=%d'>%d</a>",
               i == page_num ? " active" : "", APP_NAME, w.w_price, w.w_distance, w.w_facility, i, i);
    }
    if (page_num < total_pages) {
        printf("<a class='page-btn' href='%s?page=algorithm&w_price=%.2f&w_dist=%.2f&w_fac=%.2f&pg=%d'>ถัดไป &rarr;</a>",
               APP_NAME, w.w_price, w.w_distance, w.w_facility, page_num + 1);
    }
    printf("</div>");
}

// แสดงป้ายสิ่งอำนวยความสะดวก 3 อันแรก + "+N"
static void render_fac_tags(const char *list) {
    char buf[256];
    strncpy(buf, list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int shown = 0, extra = 0;
    char *tok = strtok(buf, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        if (shown < 3) {
            printf("<span class='tag'>%s</span>", tok);
            shown++;
        } else {
            extra++;
        }
        tok = strtok(NULL, ",");
    }
    if (extra > 0) printf("<span class='tag'>+%d</span>", extra);
}

static int near_w(double a, double b) {
    double d = a - b;
    return d < 0.005 && d > -0.005;
}

// 2. หน้ารายการหอพัก สไตล์ Agoda (`?page=algorithm&pg=N`)
static void render_algorithm_page(MaxHeap heap, int page_num, int total_pages, Weights current_w, double avg_msec) {
    fputs("<!DOCTYPE html><html lang='th'><head><meta charset='UTF-8'>"
          "<meta name='viewport' content='width=device-width, initial-scale=1'>"
          "<title>หอพักที่ดีที่สุดใกล้มหาวิทยาลัย - KAITO DORMITORY</title>"
          "<link href='https://fonts.googleapis.com/css2?family=Prompt:wght@300;400;500;600;700&display=swap' rel='stylesheet'>"
          "<style>"
          "*{box-sizing:border-box;margin:0;padding:0;font-family:'Prompt',sans-serif}"
          "body{background:#fff;color:#2a2a2e}"
          ".container{max-width:1140px;margin:0 auto;padding:0 15px}"
          "/* แถบบนสุด + แถบค้นหา */"
          ".topbar{background:#1c2350;padding:10px 0 0;font-size:.8rem}"
          ".topbar .container{display:flex;justify-content:space-between}"
          ".topbar a{color:#cfd3ee;text-decoration:none}"
          ".topbar a:hover{color:#fff}"
          ".search-wrap{background:#1c2350;padding:12px 0 16px}"
          ".search-bar{display:flex;gap:8px;align-items:stretch}"
          ".sb-field{background:#fff;border-radius:4px;height:60px;display:flex;align-items:center;padding:0 16px;gap:10px}"
          ".sb-name{flex:1}"
          ".sb-field input,.sb-field select{border:none;outline:none;font-size:1rem;font-weight:600;width:100%;background:transparent;color:#2a2a2e}"
          ".sb-select{width:180px;flex-direction:column;align-items:flex-start;justify-content:center;gap:0}"
          ".sb-select small{color:#8a8a92;font-size:.72rem}"
          ".sb-btn{background:#5392f9;color:#fff;border:none;border-radius:4px;padding:0 34px;font-weight:600;font-size:1rem;cursor:pointer}"
          ".sb-btn:hover{background:#3f80ee}"
          ".sb-gear{background:#fff;color:#1c2350;border:none;border-radius:4px;padding:0 16px;font-weight:600;font-size:.85rem;cursor:pointer}"
          ".sb-gear:hover{background:#eef3ff}"
          "/* หัวข้อ + โครงหน้า */"
          ".page-title{font-size:1.9rem;font-weight:700;margin:26px 0 24px}"
          ".layout{display:flex;gap:20px;align-items:flex-start;padding-bottom:50px}"
          ".sidebar{width:258px;flex-shrink:0;border:1px solid #e6e6e6;border-radius:4px;padding:20px}"
          ".sidebar h3{font-size:1rem;font-weight:700;margin-bottom:14px}"
          ".grp{margin-bottom:26px}.grp:last-child{margin-bottom:0}"
          ".chk{display:flex;align-items:center;gap:10px;margin-bottom:12px;font-size:.95rem;cursor:pointer}"
          ".chk input{width:22px;height:22px;accent-color:#5392f9;cursor:pointer}"
          ".content{flex:1;min-width:0}"
          "/* แท็บเรียงลำดับ */"
          ".tabs{display:flex;border:1px solid #e6e6e6;border-radius:4px;overflow:hidden;margin-bottom:24px}"
          ".tab{flex:1;text-align:center;padding:21px 8px;font-weight:600;font-size:.95rem;color:#2a2a2e;text-decoration:none;border-right:1px solid #e6e6e6}"
          ".tab:last-child{border-right:none}"
          ".tab:hover{background:#f3f7ff}"
          ".tab.active{background:#5392f9;color:#fff}"
          "/* การ์ดหอพักแนวนอน */"
          ".hcard{display:flex;border:1px solid #e6e6e6;border-radius:4px;margin-bottom:24px;background:#fff;overflow:hidden}"
          ".hc-img{position:relative;width:335px;flex-shrink:0;padding:20px 0 20px 20px}"
          ".hc-img img{width:315px;height:250px;object-fit:cover;border-radius:2px;display:block}"
          ".rank{position:absolute;top:20px;left:20px;background:#5392f9;color:#fff;font-size:.75rem;font-weight:700;padding:4px 9px;border-bottom-right-radius:4px;z-index:2}"
          ".hc-info{flex:1;padding:20px;min-width:0}"
          ".hc-title{font-size:1.3rem;font-weight:700;line-height:1.4;margin-bottom:10px}"
          ".hc-meta{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-bottom:12px;font-size:.85rem}"
          ".stars{color:#f5a623;letter-spacing:1px}"
          ".stars .off{color:#d8d8d8}"
          ".loc{color:#5392f9}"
          ".tags{display:flex;gap:6px;flex-wrap:wrap}"
          ".tag{border:1px solid #e0e0e0;border-radius:3px;padding:4px 9px;font-size:.8rem;font-weight:600}"
          ".hc-side{width:270px;flex-shrink:0;border-left:1px solid #e6e6e6;padding:20px;display:flex;flex-direction:column;justify-content:space-between;align-items:flex-end}"
          ".sc-row{display:flex;align-items:center;gap:10px}"
          ".sc-text{text-align:right}"
          ".sc-text b{display:block;font-size:.95rem}"
          ".sc-text small{color:#777;font-size:.75rem}"
          ".sc{border:2px solid #5392f9;color:#5392f9;border-radius:4px 4px 4px 0;font-size:1.2rem;padding:4px 9px;min-width:46px;text-align:center}"
          ".price-blk{text-align:right;margin-top:14px}"
          ".price-blk small{color:#777;font-size:.75rem}"
          ".price{font-size:1.6rem;font-weight:700;color:#d32f2f;line-height:1.2}"
          ".view-btn{width:100%;background:#5392f9;color:#fff;border:none;border-radius:4px;padding:16px;font-weight:600;font-size:1rem;cursor:pointer;box-shadow:0 2px 6px rgba(83,146,249,.4);margin-top:14px}"
          ".view-btn:hover{background:#3f80ee}"
          ".empty{display:none;text-align:center;padding:60px 0;color:#777}"
          "/* ปุ่มเปลี่ยนหน้า */"
          ".pagination{display:flex;justify-content:center;gap:8px;margin:10px 0 30px;flex-wrap:wrap}"
          ".page-btn{background:#fff;border:1px solid #e6e6e6;color:#333;padding:8px 14px;border-radius:3px;text-decoration:none;font-size:.85rem}"
          ".page-btn:hover{color:#5392f9;border-color:#5392f9}"
          ".page-btn.active{background:#5392f9;color:#fff;border-color:#5392f9}"
          "/* Modal */"
          ".modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.6);align-items:center;justify-content:center;z-index:1000}"
          ".modal-box{background:#fff;padding:25px;border-radius:6px;max-width:480px;width:90%;position:relative;box-shadow:0 10px 25px rgba(0,0,0,.2)}"
          ".close-icon{position:absolute;top:15px;right:20px;font-size:1.5rem;cursor:pointer;color:#888}"
          ".modal-img{width:100%;height:200px;object-fit:cover;border-radius:4px;margin-bottom:15px}"
          ".fac-chip{display:inline-block;background:#eef4ff;border:1px solid #c9dcff;color:#3f80ee;padding:4px 10px;border-radius:3px;font-size:.8rem;margin:3px}"
          ".filter-group{margin-bottom:18px}"
          ".filter-label{font-size:.85rem;color:#444;display:flex;justify-content:space-between;margin-bottom:6px;font-weight:500}"
          ".filter-slider{width:100%;accent-color:#5392f9;cursor:pointer}"
          ".apply-btn{width:100%;background:#5392f9;color:#fff;border:none;padding:12px;font-weight:600;border-radius:4px;cursor:pointer;font-size:.95rem;margin-top:10px}"
          ".apply-btn:hover{background:#3f80ee}"
          "/* มือถือ / จอแคบ */"
          "@media(max-width:900px){"
          ".layout{flex-direction:column}.sidebar{width:100%}"
          ".hcard{flex-direction:column}"
          ".hc-img{width:100%;padding:0}.hc-img img{width:100%;height:220px}.rank{top:0;left:0}"
          ".hc-side{width:100%;border-left:none;border-top:1px solid #e6e6e6;align-items:stretch}"
          ".search-bar{flex-wrap:wrap}.sb-name{flex-basis:100%}.sb-select{flex:1;width:auto}"
          ".tabs{flex-wrap:wrap}.tab{flex-basis:50%}"
          "}"
          "</style></head><body>", stdout);

    printf("<div class='topbar'><div class='container'>"
           "<a href='%s?page=algorithm'>KAITO DORMITORY</a>"
           "<span style='color:#cfd3ee;'>⏱️ เวลาเฉลี่ยประมวลผล: %.6f ms</span>"
           "</div></div>", APP_NAME, avg_msec);

    fputs("<div class='search-wrap'><div class='container search-bar'>"
          "<div class='sb-field sb-name'><span>🔍</span><input id='q' type='text' placeholder='ค้นหาชื่อหอพัก'></div>"
          "<div class='sb-field sb-select'><small>ราคาสูงสุด / เทอม</small>"
          "<select id='maxPrice'><option value='99999'>ทุกราคา</option><option value='12000'>ไม่เกิน 12,000</option>"
          "<option value='14000'>ไม่เกิน 14,000</option><option value='15000'>ไม่เกิน 15,000</option></select></div>"
          "<div class='sb-field sb-select'><small>ระยะทางสูงสุด</small>"
          "<select id='maxDist'><option value='99'>ทุกระยะ</option><option value='1'>ไม่เกิน 1 กม.</option>"
          "<option value='2'>ไม่เกิน 2 กม.</option><option value='3'>ไม่เกิน 3 กม.</option></select></div>"
          "<button class='sb-btn' onclick='applyFilters()'>ค้นหา</button>"
          "<button class='sb-gear' onclick='openAlgoModal()'>⚙️ ปรับน้ำหนัก</button>"
          "</div></div>", stdout);

    int start = (page_num - 1) * PER_PAGE;
    int count = heap.size - start;
    if (count > PER_PAGE) count = PER_PAGE;
    if (count < 0) count = 0;

    printf("<div class='container'><h1 class='page-title' id='result-title'>%d หอพักที่ดีที่สุดใกล้มหาวิทยาลัย <small style='font-size:0.95rem;color:#666;font-weight:400;'>(เวลาเฉลี่ย: %.6f ms)</small></h1>", count, avg_msec);
    fputs("<div class='layout'>", stdout);

    // ตัวกรองด้านซ้าย
    fputs("<aside class='sidebar'>"
          "<div class='grp'><h3>ระดับสิ่งอำนวยความสะดวก</h3>"
          "<label class='chk'><input type='checkbox' data-g='fac' value='9-10'> ครบครัน (9-10)</label>"
          "<label class='chk'><input type='checkbox' data-g='fac' value='7-8'> ดีมาก (7-8)</label>"
          "<label class='chk'><input type='checkbox' data-g='fac' value='5-6'> ปานกลาง (5-6)</label>"
          "<label class='chk'><input type='checkbox' data-g='fac' value='1-4'> พื้นฐาน (1-4)</label></div>"
          "<div class='grp'><h3>ช่วงราคา</h3>"
          "<label class='chk'><input type='checkbox' data-g='price' value='0-12000'> ไม่เกิน 12,000</label>"
          "<label class='chk'><input type='checkbox' data-g='price' value='12000-14000'> 12,000 - 14,000</label>"
          "<label class='chk'><input type='checkbox' data-g='price' value='14000-99999'> 14,000 ขึ้นไป</label></div>"
          "<div class='grp'><h3>ระยะทางจากมหาวิทยาลัย</h3>"
          "<label class='chk'><input type='checkbox' data-g='dist' value='0-1'> ไม่เกิน 1 กม.</label>"
          "<label class='chk'><input type='checkbox' data-g='dist' value='1.01-2'> 1 - 2 กม.</label>"
          "<label class='chk'><input type='checkbox' data-g='dist' value='2.01-99'> มากกว่า 2 กม.</label></div>"
          "</aside>", stdout);

    fputs("<main class='content'>", stdout);

    // แท็บเรียงลำดับ
    const char *tab_labels[4] = {"แนะนำ", "ราคาต่ำสุดก่อน", "ใกล้ ม. ที่สุด", "สิ่งอำนวยความสะดวกครบ"};
    double tab_w[4][3] = {{0.4, 0.4, 0.2}, {0.8, 0.1, 0.1}, {0.1, 0.8, 0.1}, {0.1, 0.1, 0.8}};
    fputs("<div class='tabs'>", stdout);
    for (int t = 0; t < 4; t++) {
        int active = near_w(current_w.w_price, tab_w[t][0]) &&
                     near_w(current_w.w_distance, tab_w[t][1]) &&
                     near_w(current_w.w_facility, tab_w[t][2]);
        printf("<a class='tab%s' href='%s?page=algorithm&w_price=%.1f&w_dist=%.1f&w_fac=%.1f'>%s</a>",
               active ? " active" : "", APP_NAME, tab_w[t][0], tab_w[t][1], tab_w[t][2], tab_labels[t]);
    }
    fputs("</div>", stdout);

    fputs("<div id='list'>", stdout);

    for (int i = 0; i < start && heap.size > 0; i++) heap_pop(&heap);

    int rank = start + 1;
    int end  = start + PER_PAGE;
    while (heap.size > 0 && rank <= end) {
        HeapNode node = heap_pop(&heap);
        double s10 = node.score * 10.0;
        const char *label = s10 >= 8.0 ? "ยอดเยี่ยม" : (s10 >= 6.5 ? "ดีมาก" : (s10 >= 5.0 ? "ดี" : "พอใช้"));
        int stars = (node.dorm.facilities_score + 1) / 2;
        if (stars > 5) stars = 5;

        printf("<div class='hcard' data-name=\"%s\" data-price='%d' data-dist='%.1f' data-fac='%d'>",
               node.dorm.name, node.dorm.price, node.dorm.distance, node.dorm.facilities_score);

        printf("<div class='hc-img'><span class='rank'>อันดับ %d%s</span><img src='%s' alt='' onerror=\"this.src='https://images.unsplash.com/photo-1555854877-bab0e564b8d5?w=500'\"></div>",
               rank, rank == 1 ? " 🔥" : "", node.dorm.image_url);

        printf("<div class='hc-info'><h2 class='hc-title'>%s</h2>", node.dorm.name);
        printf("<div class='hc-meta'><span class='stars'>");
        for (int i = 0; i < 5; i++) {
            if (i < stars) printf("★");
            else printf("<span class='off'>★</span>");
        }
        printf("</span><span class='loc'>📍 ห่าง %.1f กม. จากมหาวิทยาลัย</span></div>", node.dorm.distance);
        printf("<div class='tags'>");
        render_fac_tags(node.dorm.facilities_list);
        printf("</div></div>");

        printf("<div class='hc-side'>");
        printf("<div class='sc-row'><div class='sc-text'><b>%s</b><small>คะแนนจากอัลกอริทึม</small></div><div class='sc'>%.1f</div></div>",
               label, s10);
        printf("<div style='width:100%%'>");
        printf("<div class='price-blk'><small>ราคาเริ่มต้น</small><div class='price'>฿%d</div><small>/เทอม</small></div>",
               node.dorm.price);
        printf("<button class='view-btn' onclick=\"openModal('%s','%s','%d','%.1f','%s')\">ดูรายละเอียด</button>",
               node.dorm.name, node.dorm.image_url, node.dorm.price, node.dorm.distance, node.dorm.facilities_list);
        printf("</div></div>");

        printf("</div>");
        rank++;
    }

    fputs("</div>", stdout);
    fputs("<div class='empty' id='empty'>ไม่พบหอพักที่ตรงกับเงื่อนไขที่เลือก ลองปรับตัวกรองดูนะครับ</div>", stdout);

    if (total_pages > 1) render_pagination(page_num, total_pages, current_w);

    fputs("</main></div></div>", stdout);

    int p_val = (int)(current_w.w_price * 100 + 0.5);
    int d_val = (int)(current_w.w_distance * 100 + 0.5);
    int f_val = (int)(current_w.w_facility * 100 + 0.5);

    fputs("<div id='algoModal' class='modal'><div class='modal-box'>"
          "<span class='close-icon' onclick='closeAlgoModal()'>&times;</span>"
          "<h3 style='font-size:1.1rem;margin-bottom:6px;color:#3f80ee'>⚙️ ปรับแต่งค่าน้ำหนัก Algorithm</h3>"
          "<p style='font-size:.8rem;color:#666;margin-bottom:20px'>ปรับสไลเดอร์เพื่อเปลี่ยนน้ำหนักในการคำนวณอันดับ Max-Heap</p>", stdout);

    printf("<div class='filter-group'><div class='filter-label'><span>💰 น้ำหนักราคา (Price Weight)</span><b id='val-price'>%d%%</b></div>"
           "<input type='range' id='w-price' class='filter-slider' min='0' max='100' value='%d' oninput='updateLabels()'></div>", p_val, p_val);
    printf("<div class='filter-group'><div class='filter-label'><span>📍 น้ำหนักระยะทาง (Distance Weight)</span><b id='val-dist'>%d%%</b></div>"
           "<input type='range' id='w-dist' class='filter-slider' min='0' max='100' value='%d' oninput='updateLabels()'></div>", d_val, d_val);
    printf("<div class='filter-group'><div class='filter-label'><span>⭐ น้ำหนักสิ่งอำนวยความสะดวก (Facility Weight)</span><b id='val-fac'>%d%%</b></div>"
           "<input type='range' id='w-fac' class='filter-slider' min='0' max='100' value='%d' oninput='updateLabels()'></div>", f_val, f_val);

    fputs("<button class='apply-btn' onclick='applyAlgoFilter()'>บันทึกและคำนวณใหม่ ⚡</button>"
          "</div></div>", stdout);

    fputs("<div id='dormModal' class='modal'><div class='modal-box'>"
          "<span class='close-icon' onclick='closeModal()'>&times;</span>"
          "<img id='modal-img' class='modal-img' src='' alt=''>"
          "<h3 id='modal-title' style='font-size:1.2rem;margin-bottom:6px'></h3>"
          "<p style='font-size:1.1rem;color:#d32f2f;font-weight:600;margin-bottom:10px'>฿<span id='modal-price'></span> /เทอม</p>"
          "<p style='font-size:.85rem;color:#666;margin-bottom:15px'>📍 ระยะทาง: <b id='modal-dist'></b> กม. จากมหาวิทยาลัย</p>"
          "<h4 style='font-size:.85rem;color:#333;margin-bottom:8px'>สิ่งอำนวยความสะดวก</h4>"
          "<div id='modal-facs'></div>"
          "</div></div>", stdout);

    fputs("<script>"
          "function openModal(name,img,price,dist,facs){"
          "document.getElementById('modal-title').innerText=name;"
          "document.getElementById('modal-img').src=img;"
          "document.getElementById('modal-price').innerText=price;"
          "document.getElementById('modal-dist').innerText=dist;"
          "let html='';"
          "facs.split(',').forEach(i=>{html+=`<span class='fac-chip'>✓ ${i.trim()}</span>`;});"
          "document.getElementById('modal-facs').innerHTML=html;"
          "document.getElementById('dormModal').style.display='flex';}"
          "function closeModal(){document.getElementById('dormModal').style.display='none';}"
          "function openAlgoModal(){document.getElementById('algoModal').style.display='flex';}"
          "function closeAlgoModal(){document.getElementById('algoModal').style.display='none';}"
          "function updateLabels(){"
          "document.getElementById('val-price').innerText=document.getElementById('w-price').value+'%';"
          "document.getElementById('val-dist').innerText=document.getElementById('w-dist').value+'%';"
          "document.getElementById('val-fac').innerText=document.getElementById('w-fac').value+'%';}"
          "window.onclick=function(e){"
          "if(e.target==document.getElementById('dormModal'))closeModal();"
          "if(e.target==document.getElementById('algoModal'))closeAlgoModal();};"
          "function inRange(v,spec){const p=spec.split('-').map(Number);return v>=p[0]&&v<=p[1];}"
          "function applyFilters(){"
          "const q=document.getElementById('q').value.trim().toLowerCase();"
          "const maxP=Number(document.getElementById('maxPrice').value);"
          "const maxD=Number(document.getElementById('maxDist').value);"
          "const groups=['fac','price','dist'].map(g=>Array.from(document.querySelectorAll(\"input[data-g='\"+g+\"']:checked\")).map(c=>c.value));"
          "let shown=0;"
          "document.querySelectorAll('.hcard').forEach(c=>{"
          "const price=Number(c.dataset.price),dist=Number(c.dataset.dist),fac=Number(c.dataset.fac);"
          "let ok=c.dataset.name.toLowerCase().includes(q)&&price<=maxP&&dist<=maxD;"
          "const vals=[fac,price,dist];"
          "groups.forEach((g,i)=>{if(g.length&&!g.some(s=>inRange(vals[i],s)))ok=false;});"
          "c.style.display=ok?'flex':'none';if(ok)shown++;});"
          "document.getElementById('result-title').innerText=shown+' หอพักที่ดีที่สุดใกล้มหาวิทยาลัย';"
          "document.getElementById('empty').style.display=shown?'none':'block';}"
          "document.querySelectorAll('.sidebar input').forEach(i=>i.addEventListener('change',applyFilters));"
          "document.getElementById('q').addEventListener('input',applyFilters);"
          "document.getElementById('maxPrice').addEventListener('change',applyFilters);"
          "document.getElementById('maxDist').addEventListener('change',applyFilters);", stdout);

    printf("function applyAlgoFilter(){"
           "let p=(document.getElementById('w-price').value/100).toFixed(2);"
           "let d=(document.getElementById('w-dist').value/100).toFixed(2);"
           "let f=(document.getElementById('w-fac').value/100).toFixed(2);"
           "window.location.href=`%s?page=algorithm&w_price=${p}&w_dist=${d}&w_fac=${f}`;}", APP_NAME);

    fputs("</script></body></html>", stdout);
}

int main(void) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    printf("Content-Type: text/html; charset=utf-8\r\n\r\n");
    fflush(stdout);

    Dorm dorms[] = {
        {1,  "หอพักกรีนปาร์ค",     12000, 3.1, 3,  "แอร์, พัดลม, ทีวี, โต๊ะทำงาน, โต๊ะเครื่องแป้ง, เตียง, ตู้เสื้อผ้า, ตู้เย็น, เครื่องทำน้ำอุ่น", "/images/dorm1.jpg"},
        {2,  "หอพักบ้านเช่ามยุรี",       12000, 1.5, 9,  "แอร์, พัดลม, โต๊ะทำงาน, เตียง, ตู้เสื้อผ้า, ตู้เย็น, เครื่องทำน้ำอุ่น, ซิงค์ล้างจาน", "/images/dorm2.jpg"},
        {3,  "หอพักอันนา",              15500, 2.9, 7,  "แอร์, พัดลม, ทีวี, โต๊ะเครื่องแป้ง, โต๊ะทำงาน, เตียง, ตู้เสื้อผ้า, ตู้เย็น, เครื่องทำน้ำอุ่น, ซิงค์ล้างจาน", "/images/dorm3.jpg"},
        {4,  "หอพักคำเมือง",               16000, 2.1, 2,  "แอร์, เตียง, โต๊ะเครื่องแป้ง, ตู้เสื้อผ้า, โต๊ะทำงาน, ทีวี, เครื่องทำน้ำอุ่น", "/images/dorm4.jpg"},
        {5,  "หอพักเชรวี",                  15000, 0.7, 8,  "แอร์, ทีวี, เตียง, ตู้เสื้อผ้า, โต๊ะอ่านหนังสือ, ตู้เย็น, เครื่องทำน้ำอุ่น", "/images/dorm5.jpg"},
        {6,  "หอพักคิตตี้",     12500, 1.5, 5,  "แอร์, เตียง, ตู้เสื้อผ้า, ตู้เย็น, เครื่องทำน้ำอุ่น, ซิงค์ล้างจาน", "/images/dorm6.jpg"},
        {7,  "หอพักบ้านสวนพุฒศรี",        13000, 2.5, 10, "แอร์, พัดลม, ทีวี, โต๊ะเครื่องแป้ง, โต๊ะทำงาน, เตียง, ตู้เสื้อผ้า, ตู้เย็น, เครื่องทำน้ำอุ่น, ซิงค์ล้างจาน", "/images/dorm7.jpg"},
        {8,  "หอพักปรีชา",      12000, 2.0, 4,  "แอร์, โต๊ะทำงาน, เตียง, ตู้เสื้อผ้า, ตู้เย็น, เครื่องทำน้ำอุ่น, ซิงค์ล้างจาน", "/images/dorm8.jpg"},
        {9,  "หอพักแก้วสุวรรณ",               15000, 2.2, 6,  "แอร์, พัดลม, ทีวี, โต๊ะเครื่องแป้ง, โต๊ะทำงาน, เตียง, ตู้เสื้อผ้า, ตู้เย็น, เครื่องทำน้ำอุ่น, ซิงค์ล้างจาน", "/images/dorm9.jpg"},
        {10, "หอพักทอฝัน",      11000, 3.2, 7,  "แอร์, พัดลม, ทีวี, โต๊ะเครื่องแป้ง, โต๊ะทำงาน, เตียง, ตู้เสื้อผ้า, ตู้เย็น, เครื่องทำน้ำอุ่น, ซิงค์ล้างจาน", "/images/dorm10.jpg"},
    };
    int n = sizeof(dorms) / sizeof(dorms[0]);

    char *query_string = getenv("QUERY_STRING");

    Weights w;
    w.w_price    = get_param_double(query_string, "w_price", 0.4);
    w.w_distance = get_param_double(query_string, "w_dist",  0.4);
    w.w_facility = get_param_double(query_string, "w_fac",   0.2);

    MaxHeap heap;

    // =========================================================
    // --- เริ่มจับเวลาค้นหา/คำนวณคะแนน (ตามสไลด์) ---
    // =========================================================
    clock_t start, end;
    double total_sec, avg_msec;

    start = clock();                             /* เริ่มจับเวลา */
    for (int i = 0; i < REPEAT; i++) {
        compute_scores(dorms, n, w, &heap);      /* คำนวณคะแนนและสร้าง Max-Heap 1,000 รอบ */
    }
    end = clock();                               /* หยุดจับเวลา */

    total_sec = (double)(end - start) / CLOCKS_PER_SEC;
    avg_msec = (total_sec * 1000.0) / REPEAT;    /* เวลาเฉลี่ยต่อรอบ (มิลลิวินาที) */
    // =========================================================

    int total_pages = (n + PER_PAGE - 1) / PER_PAGE;
    int page_num = (int)get_param_double(query_string, "pg", 1);
    if (page_num < 1) page_num = 1;
    if (page_num > total_pages) page_num = total_pages;

    // ส่งค่า avg_msec ไปแสดงบนหน้าเว็บ
    render_algorithm_page(heap, page_num, total_pages, w, avg_msec);

    return 0;
}