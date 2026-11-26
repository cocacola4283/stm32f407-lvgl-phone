/**
 ****************************************************************************************************
 * @file        app_shell.c
 * @brief       婵炴垶鎸诲Σ鎺曘亹瀹ュ纭€闁哄浂浜炵粈鍒琌S 婵＄偛顑呯€涒晠鎮ч幖浣告嵍闁糕剝鍑瑰锟犳煕閵壯冧壕闁绘牭缍侀弫? 闁?x 3 闂佸憡甯楅〃鎰? app 闂佸憡甯掑ú锕€鐣? ****************************************************************************************************
 */

#include "app_shell.h"
#include "gui_2048.h"
#include "app_files.h"
#include "app_photos.h"
#include "app_icons.h"
#include "lv_port_disp.h"
#include "./SYSTEM/sys/sys.h"
#include "./BSP/SD/lv_fs_fat.h"
#include <stdint.h>

/* 闁搞儳鍋撻悥锝夊级閵夛妇鐖遍柨? = 濞?SD 闁告銈庡殺 I*.BIN闁挎稑鐗嗚ぐ鏌ユ⒕韫囨梹顦ч柟骞垮灩濞存﹢寮介崶椋庣濞达絽妫欓惁锛勬暜瑜旈崗妯兼啺娴ｇ瓔鍤㈤柛妞绘缁辨繂顭ㄩ幋婵喰楀ù鍏肩鐢偓閻㈩垎宥囩
 *           0 = 闁活潿鍔岄崬鏉戠暤鐏炶姤韬?Flash 闂佹彃鐬煎▓鎴﹀礈椤栨稒鎷遍柨娑樼墦濞?IO闁挎稑鏈〒鎯扮疀椤愵剛绀夐柟鎭掑姀瀹曟﹢鏁?*/
#define ICONS_FROM_SD   0

#define APP_2048        1
#define APP_FILES       2
#define APP_TODO        3
#define APP_SETTINGS    4
#define APP_MUSIC       5
#define APP_ABOUT       6

/* iOS 婵＄偛顑呯€涒晠鎮ч幖浣哥倞闁绘劦鍓涢崹濂告倶韫囨挻鎯堟い?*/
#if ICONS_FROM_SD
#define ICON_DSC(x)     NULL              /* 从 SD 读，不再引用内嵌数组 → 省 84KB Flash */
#else
#define ICON_DSC(x)     &(x)
#endif

#define ICON_W          84
#define ICON_R          19          /* 闂侀潻绠戝Λ婊堬綖?闂?22% */
#define COL_X0          12
#define COL_DX          106
#define ROW_Y0          34
#define ROW_DY          104

static volatile uint8_t g_photo_req = 0;   /* 1 = 用户点了 PHOTO，待主循环处理 */
static lv_obj_t *g_home          = NULL;
static lv_obj_t *g_scr_2048      = NULL;
static lv_obj_t *g_scr_files     = NULL;
static lv_obj_t *g_scr_todo      = NULL;
static lv_obj_t *g_scr_settings  = NULL;
static lv_obj_t *g_scr_music     = NULL;
static lv_obj_t *g_scr_about     = NULL;

/* ---------------- 闂佹悶鍎抽崑鐘绘儍閻旂厧绀岄柛娑㈠亰濡?---------------- */
static void back_cb(lv_event_t *e)
{
    (void)e;
    app_shell_home();
}

static void rst_cb(lv_event_t *e)
{
    (void)e;
    lv_refr_now(NULL);
    NVIC_SystemReset();
}

/* ---------------- 闁诲繒鍋涚换鎰不閻戣棄绀嗛柛銉ｅ妼鎼?---------------- */
void app_shell_open(lv_obj_t *scr)
{
    if (scr == NULL)
    {
        return;
    }
    lv_screen_load(scr);          /* 閻剚妞傞崚鍥ㄥ床 */
}

void app_shell_home(void)
{
    if (g_home == NULL)
    {
        return;
    }
    lv_screen_load(g_home);
}

/* ---------------- 闂備緡鍋呭銊╁极閵堝洣鐒婇煫鍥ㄧ☉閸斻儱霉?---------------- */
lv_obj_t *app_make_back_btn(lv_obj_t *scr)
{
    lv_obj_t *b = lv_button_create(scr);
    lv_obj_t *l;

    lv_obj_set_size(b, 62, 28);
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -4, 2);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x5C6BC0), LV_PART_MAIN);
    lv_obj_add_event_cb(b, back_cb, LV_EVENT_PRESSED, NULL);

    l = lv_label_create(b);
    lv_label_set_text(l, "BACK");
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(l);

    return b;
}

lv_obj_t *app_make_placeholder(const char *title)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *l;

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x263238), LV_PART_MAIN);

    l = lv_label_create(scr);
    lv_label_set_text(l, title);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, -12);

    l = lv_label_create(scr);
    lv_label_set_text(l, "Coming soon...");
    lv_obj_set_style_text_color(l, lv_color_hex(0x90A4AE), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 16);

    app_make_back_btn(scr);
    return scr;
}

/* 闂佸憡甯囬崐鏍蓟?S: 闂佺儵鏅滅湁缂佹鍊垮鍨緞鐎ｎ剚鈷曢梺鍝勭墕椤﹀磭鎹㈤弽銊р枖妞ゅ繐鎳忛悗顔济归悩渚晣缂佽鲸鐟╁鍨緞婵犲啰娈ら梺娲绘娇閸斿绱為崨鏉戠倞闁诡垎鍕洯闂佸搫绉村ù鍕濠靛棌鏌﹂柍鈺佸暞缁犳帡鎮樿箛姘惈闁轰降鍊濋幆宀勫閵堝懏顏￠梺鎼炲劤閸嬫捇鎮ラ敐澶嬫櫖?*/
static uint8_t icon_file_exists(const char *path)
{
    lv_fs_file_t f;

    if (lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK)
    {
        return 0;
    }
    lv_fs_close(&f);
    return 1;
}
/* ---------------- 闂佹悶鍎抽崑鎾绘偉閿濆鍊烽柣鐔告緲濮?---------------- */
static void icon_pressed(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_set_style_bg_opa(b, 60, LV_PART_MAIN);
}

static void icon_cb(lv_event_t *e)
{
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *b = lv_event_get_target(e);

    lv_obj_set_style_bg_opa(b, 0, LV_PART_MAIN);    /* 闂佸搫顦遍崑鎾存櫠?闁荤姴鎼悿鍥归崱娑樼鐎广儱妫旈崚鎺戭熆?*/

    switch (id)
    {
        case APP_2048:
            if (g_scr_2048 == NULL) { g_scr_2048 = app_2048_create(); }
            app_shell_open(g_scr_2048);
            break;

        case APP_FILES:
            if (g_scr_files == NULL) { g_scr_files = app_files_create(); }
            app_shell_open(g_scr_files);
            break;

        case APP_TODO:
            if (g_scr_todo == NULL) { g_scr_todo = app_make_placeholder("TO DO"); }
            app_shell_open(g_scr_todo);
            break;

        case APP_SETTINGS:
            if (g_scr_settings == NULL) { g_scr_settings = app_make_placeholder("SETTINGS"); }
            app_shell_open(g_scr_settings);
            break;

        case APP_MUSIC:
            /* 闁搞儱澧芥晶鏍硅箛姘兼綌闁革綆鐓夌槐鐗堟交濞嗘挸娅￠柛娆樹簼婢э箓寮介崶顏嶅敹闁挎稑鐬煎﹢鈥愁潰閿濆牏绠ラ悶娑樺濮橈妇绱掑▎搴＄槣鐎甸偊浜為獮鍡涙晬閸粎鐟濋柤瀹犳濠€?LVGL 婵炴挸寮堕悡瀣▔椤撯懇鍋撻弮鈧慨鐘典沪韫囨挾顔庨柨?*/
            g_photo_req = 1;
            break;

        case APP_ABOUT:
            if (g_scr_about == NULL) { g_scr_about = app_make_placeholder("ABOUT"); }
            app_shell_open(g_scr_about);
            break;

        default:
            break;
    }
}

/* 闂備緡鍋嗘慨宕囩博鐎涙鈻?iOS 婵＄偛顑呯€涒晠鎮ч幖浣哥倞闁绘劦鍓涢崹濂告煥濞戞瑨澹樻繛鎾冲⒔閹叉挳骞掗幘瀵稿幍闂?+ 婵炴垶鎸搁敃锝囩箔閸涱厹鈧帡骞囬鈧紞?+ 闂佺鍩栧ú鏍ㄧ附?+ 闂佽皫鍐ｅ亾閻斿妫屾繝銏犵垻閸滀礁鍓ㄩ梺?+ 婵炴垶鎸搁鍡涘蓟閻旂厧瑙︾€广儱娉?*/
static void make_icon(lv_obj_t *parent, int32_t x, int32_t y,
                      const char *sym, const char *name, uint32_t id,
                      uint32_t c_top, uint32_t c_bottom, const lv_image_dsc_t *dsc, const char *sd_path)
{
    lv_obj_t *b;
    lv_obj_t *q;
    lv_obj_t *l;

    /* 闂佸憡鐟崹鎶藉磻閿濆绀勯悹鍥ㄥ絻闂呮﹢鏌涢埡鍐ㄦ珢缂佽鲸鐟╁畷銉╊敃閿涘嫮鎲梺鍝勫€介～澶愬几閸愵亖鍋撳☉娆樻缂?*/
    b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, ICON_W, ICON_W + 20);
    lv_obj_set_style_bg_opa(b, 0, LV_PART_MAIN);          /* 闂佺厧顨庢禍锝夋閳哄懏鐒婚煫鍥ㄦ⒐椤牠鏌ㄥ☉妯肩劮妞ゎ偄顑囬幉瀛樺緞鐏炵偓娈搁梺鍛婂姇閹冲酣鎯囬幍顔藉闁绘棁宕甸悡?*/
    lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, icon_pressed, LV_EVENT_PRESSED, (void *)(uintptr_t)id);
    lv_obj_add_event_cb(b, icon_cb, LV_EVENT_RELEASED, (void *)(uintptr_t)id);

    /* 闂佸憡鍔曢幊搴ㄦ儑娴煎瓨鏅慨姗嗗墯婵″洭鎮峰▎鎰瑲闁哄瞼鍠栧畷?+ 濠电偞鎸婚崝鏇°亹?+ 闂佺鍩栧ú鏍ㄧ附?*/
    q = lv_obj_create(b);
    lv_obj_set_pos(q, 0, 0);
    lv_obj_set_size(q, ICON_W, ICON_W);
    lv_obj_set_style_border_width(q, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(q, 0, LV_PART_MAIN);
    lv_obj_clear_flag(q, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(q, LV_OBJ_FLAG_CLICKABLE);      /* 闁绘劗鎳撻崵顔界閵堝洨鑸跺鑸电墪閻即骞愭径鎰唉 */

#if ICONS_FROM_SD
    if ((sd_path != NULL) && icon_file_exists(sd_path))
    {
        lv_obj_set_style_bg_opa(q, 0, LV_PART_MAIN);
        l = lv_image_create(q);
        lv_image_set_src(l, sd_path);
        lv_obj_center(l);
    }
    else
#endif
    if (dsc != NULL)
    {
        /* 濞达絽绉村ù姗€宕堕悙顒傚灱闁挎稒鑹惧〒鍓ф喆?婵炴挻鍔曡ぐ?闂傚啯娼欐總鏍焾閻ｅ苯鍔嬮弶鈺傜☉濞存﹢鏌岀仦鑲╁晩闁挎稑鐭佺换鏍煂鐏炶棄娑ч悹鎰綑濞?*/
        lv_obj_set_style_bg_opa(q, 0, LV_PART_MAIN);
        l = lv_image_create(q);
        lv_image_set_src(l, dsc);
        lv_obj_center(l);
    }
    else
    {
        /* 闁搞儳鍋ら埀顑藉亾闁挎稒姘ㄩ悡婵嬫煂韫囨挻绂堥柡宥呮祫缁辨瑥銆掗幇顒€缍侀柛锕€妫滈～?+ 閻庢稒銇炵紞瀣箔閿曗偓瑜板潡鏁?*/
        lv_obj_set_style_radius(q, ICON_R, LV_PART_MAIN);
        lv_obj_set_style_bg_color(q, lv_color_hex(c_top), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(q, lv_color_hex(c_bottom), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(q, LV_GRAD_DIR_VER, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(q, 7, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(q, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(q, 60, LV_PART_MAIN);
        lv_obj_set_style_shadow_offset_y(q, 2, LV_PART_MAIN);

        l = lv_label_create(q);
        lv_label_set_text(l, sym);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(l);
    }

    /* 婵炴垶鎸搁鍡涘蓟閻旂厧瑙︾€广儱娉?*/
    l = lv_label_create(b);
    lv_label_set_text(l, name);
    lv_obj_set_style_text_color(l, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, 0);
}

/* ---------------- 濠碘剝顨呴惌鍌氼焽?---------------- */
static void build_home(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *l;
    lv_obj_t *rst;
    lv_obj_t *r;

    g_home = scr;

    /* iOS 婵＄偛顑呯€涒晠鎮х粙澹冲孩娼忛銏╂婵犮垽鈧稖鍏岄柣鎺戯躬閺佸秴顫濋銈囨啰濠电儑绲借ぐ澶岀箔閸涱劶?*/
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B2A38), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x0E1620), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 闂佺粯顭堥崺鏍焵椤戣法鍔嶉柣鏍村櫍閺佸秴顫濋鐕佹Н闂?+ 闂佸搫绉村ú顓€?+ RST */
    l = lv_label_create(scr);
    lv_label_set_text(l, "9:41");
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(l, 10, 6);

    l = lv_label_create(scr);
    lv_label_set_text(l, "MY PHONE");
    lv_obj_set_style_text_color(l, lv_color_hex(0xB0BEC5), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 6);

    rst = lv_button_create(scr);
    lv_obj_set_size(rst, 52, 24);
    lv_obj_align(rst, LV_ALIGN_TOP_RIGHT, -6, 4);
    lv_obj_set_style_radius(rst, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rst, lv_color_hex(0xB71C1C), LV_PART_MAIN);
    lv_obj_add_event_cb(rst, rst_cb, LV_EVENT_PRESSED, NULL);
    r = lv_label_create(rst);
    lv_label_set_text(r, "RST");
    lv_obj_set_style_text_color(r, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(r);

    /* 2 闁?x 3 闂?*/
    make_icon(scr, COL_X0,            ROW_Y0,          LV_SYMBOL_VIDEO,     "2048",  APP_2048,      0xFFB74D, 0xF57C00, ICON_DSC(icon_2048), "S:/I2048.bin");
    make_icon(scr, COL_X0 + COL_DX,   ROW_Y0,          LV_SYMBOL_DIRECTORY, "FILES", APP_FILES,     0x4FC3F7, 0x0288D1, ICON_DSC(icon_files), "S:/IFILES.bin");
    make_icon(scr, COL_X0 + COL_DX*2, ROW_Y0,          LV_SYMBOL_LIST,      "TO DO", APP_TODO,      0x81C784, 0x388E3C, ICON_DSC(icon_todo), "S:/ITODO.bin");
    make_icon(scr, COL_X0,            ROW_Y0 + ROW_DY, LV_SYMBOL_SETTINGS,  "SETUP", APP_SETTINGS,  0xB0BEC5, 0x607D8B, ICON_DSC(icon_setup), "S:/ISETUP.bin");
    make_icon(scr, COL_X0 + COL_DX,   ROW_Y0 + ROW_DY, LV_SYMBOL_IMAGE,     "PHOTO", APP_MUSIC,     0xF06292, 0xC2185B, ICON_DSC(icon_photo), "S:/IPHOTO.bin");
    make_icon(scr, COL_X0 + COL_DX*2, ROW_Y0 + ROW_DY, LV_SYMBOL_FILE,      "ABOUT", APP_ABOUT,     0x9575CD, 0x5E35B1, ICON_DSC(icon_about), "S:/IABOUT.bin");
}

void app_shell_start(void)
{
    build_home();
    lv_screen_load(g_home);
}

/**
 * @brief  濞戞捁顕ч幆濠囨偝椤栨稓妲ㄩ弶鐑嗗枦閻ㄧ喖鎮介妸銈囶伇婵炲棌妲勭槐鐗堝緞閸曨厽鍊?闂傚洠鍋撻悷鏇氳兌鐎氼參宕￠悩鑼綄妤?闁?app闁挎稑鐗嗗ù姗€鎮ч崶銊с偦閻熸瑥鐗炵槐? * @note   闊洤鎳橀妴蹇涘绩閹勮含 lv_timer_handler() 濞戞柨顑呴ˇ鑽ゆ嫬閸愵亝鏆?闁炽儲鏌￠埀?濞戞挸绉烽崗姗€宕?LVGL 婵炴挸寮堕悡瀣▔椤撯懇鍋撻弮鈧慨鐘典沪韫囨挾顔? */
void app_shell_poll(void)
{
    if (!g_photo_req)
    {
        return;
    }
    g_photo_req = 0;

    lv_refr_now(NULL);                       /* 闁稿繐鐗婃俊?LVGL 闁圭鍊介幑锝夋儍閸曨垰娅㈢紓浣姑崣蹇涙焾閵娿儱鐓曢悗?*/
    lv_port_disp_wait_idle();                /* 绛?DMA 鎶婃渶鍚庝竴鍧楁惉瀹岋紝鍚﹀垯鐙崰灞忓箷浼氬啓涔?*/
    app_photos_run();                        /* 闁绘瑯鍓欏畷鎵沪韫囨挾顔庨悹鐑樺灥濞存﹢鎮ч崶銊с偦閻?*/
    lv_obj_invalidate(lv_screen_active());   /* 闁搞儳鍋炲鐢稿触鎼粹€崇箒闁告帟鍩栭弳锝囦沪韫囨稑娅㈢紓浣股戦、鎴︽?*/
}