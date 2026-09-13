/* ===================================================================
 * main.c
 * Orkestrasi: cls -> panggil semua modul detect_*() -> render sesuai
 * layout "System Properties" yang disepakati.
 *
 * Layout target:
 *
 *   About this PC
 *   -------------------------------
 *   SYSTEM   : FreeDOS 1.4  <--- atau "Burdah-DOS-System 0.x"
 *   CPU      : Intel Pentium 100 MHz
 *   MACHINE  : ASUS P/I-P55T2P4
 *   RAM      : 640 KB (Conventional) + 15 MB (Extended)
 *   DISK     : HITACHI DK223A, 450 MB
 *              PARTITION : 300 MB FAT
 *              PARTITION : 150 MB RAW
 *   MONITOR  : VGA, 800 x 600
 *
 *   [bar warna-warni]
 * =================================================================== */

#include <stdio.h>
#include <string.h>
#include "sysinfo.h"
#include "detect_os.h"
#include "detect_cpu.h"
#include "detect_mem.h"
#include "detect_disk.h"
#include "detect_board.h"
#include "detect_video.h"
#include "render.h"

/* Gabungkan field nama CPU + clock speed jadi satu baris tampilan
 * sesuai layout target ("CPU : Intel Pentium 100 MHz"). Status
 * gabungan dianggap OK hanya kalau KEDUA sub-field OK -- kalau salah
 * satu gagal (mis. clock speed belum terukur), seluruh baris tetap
 * ditampilkan tapi warnanya jadi merah, dan bagian yang gagal
 * tertulis apa adanya (mis. "Intel Pentium, belum terukur"). */
static void merge_cpu_line(const cpu_info_t *cpu, field_t *out)
{
    if (cpu->clock.status == STATUS_OK) {
        sprintf(out->text, "%s %s", cpu->name.text, cpu->clock.text);
    } else {
        sprintf(out->text, "%s, clock %s", cpu->name.text, cpu->clock.text);
    }
    out->status = (cpu->name.status == STATUS_OK &&
                   cpu->clock.status == STATUS_OK)
                  ? STATUS_OK : STATUS_FAIL;
}

/* Gabungkan field RAM conventional + extended jadi satu baris sesuai
 * layout target ("RAM : 640 KB (Conventional) + 15 MB (Extended)"). */
static void merge_ram_line(const mem_info_t *mem, field_t *out)
{
    sprintf(out->text, "%s (Conventional) + %s (Extended)",
            mem->conventional.text, mem->extended.text);
    out->status = (mem->conventional.status == STATUS_OK &&
                   mem->extended.status == STATUS_OK)
                  ? STATUS_OK : STATUS_FAIL;
}

/* Gabungkan nama drive + ukuran total jadi satu baris DISK. */
static void merge_disk_line(const disk_info_t *disk, field_t *out)
{
    sprintf(out->text, "%s, %s", disk->drive_name.text, disk->total_size.text);
    out->status = (disk->drive_name.status == STATUS_OK &&
                   disk->total_size.status == STATUS_OK)
                  ? STATUS_OK : STATUS_FAIL;
}

/* Gabungkan adapter + resolusi jadi satu baris MONITOR. Kalau
 * resolusi belum terdeteksi, cukup tampilkan nama adapter saja
 * (tidak menulis ", N/A" yang mengotori tampilan). */
static void merge_monitor_line(const video_info_t *video, field_t *out)
{
    if (video->resolution.status == STATUS_OK) {
        sprintf(out->text, "%s, %s", video->adapter.text, video->resolution.text);
    } else {
        strcpy(out->text, video->adapter.text);
    }
    out->status = video->adapter.status;
}

int main(void)
{
    sysinfo_t info;
    field_t cpu_line, ram_line, disk_line, monitor_line;
    int i;

    clear_screen();

    /* --- Kumpulkan semua data dulu (tidak ada I/O layar di sini) --- */
    detect_os(&info.os_name);
    detect_cpu(&info.cpu);
    detect_mem(&info.mem);
    detect_board(&info.board);
    detect_disk(&info.disk);
    detect_video(&info.video);

    merge_cpu_line(&info.cpu, &cpu_line);
    merge_ram_line(&info.mem, &ram_line);
    merge_disk_line(&info.disk, &disk_line);
    merge_monitor_line(&info.video, &monitor_line);

    /* --- Render --- */
    printf("About this PC\r\n");
    printf("########################################\r\n");

    render_field("SYSTEM ", &info.os_name,      0);
    render_field("CPU    ", &cpu_line,           0);
    render_field("MACHINE", &info.board.vendor_name, 0);
    render_field("RAM    ", &ram_line,            0);
    render_field("DISK   ", &disk_line,           0);

    for (i = 0; i < info.disk.partition_count; i++) {
        field_t f;
        f.status = STATUS_OK;
        sprintf(f.text, "%lu MB %s",
                info.disk.partitions[i].size_mb,
                info.disk.partitions[i].type_name);
        /* Indent 11 spasi supaya sejajar dengan posisi value DISK
         * (panjang "DISK    : " = 10 karakter + 1 spasi ekstra). */
        render_field("PARTITION", &f, 11);
    }

    render_field("MONITOR", &monitor_line, 0);

    printf("\r\n");
    render_rainbow_bar(31);

    return 0;
}
