/*
 * Experimental K-3/K-3II Live View passive PEF downloader.
 *
 * Goal: keep HDMI Live View active, let an external IR remote trigger the
 * shutter, and use USB/MSC only to pull buffer 0 as PEF.  This intentionally
 * skips normal status/settings/file-format transactions after connect.
 *
 * This is an experiment, not an upstream-supported feature.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#include <unistd.h>
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC
#endif

#include "pslr.h"
#include "pslr_log.h"
#include "pslr_utils.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--device F] [--output lvtest.pef] [--debug] [--poll] [--delete]\n"
        "\n"
        "Recommended first test (manual/minimal USB chatter):\n"
        "  1. Camera USB mode = MSC\n"
        "  2. HDMI connected, press LV and confirm live image\n"
        "  3. Run: %s --device F --output lvtest.pef --debug\n"
        "  4. Trigger ONE photo with the IR remote\n"
        "  5. Wait until HDMI live view returns, then press Enter here\n"
        "\n"
        "Options:\n"
        "  --device X   Windows camera drive letter, e.g. F\n"
        "  --output F   PEF output filename (default: lvtest.pef)\n"
        "  --debug      verbose protocol logging\n"
        "  --poll       poll only the lightweight buffer-mask command instead of Enter\n"
        "  --delete     delete camera buffer 0 after a successful download\n",
        prog, prog);
}

static int save_pef_direct(pslr_handle_t cam, const char *filename) {
    uint8_t buf[65536];
    uint32_t total = 0;
    uint32_t expected = 0;
    int fd;
    int ret;

    /* PEF does not use JPEG resolution; 0 is intentional. */
    ret = pslr_buffer_open_direct(cam, 0, PSLR_BUF_PEF, 0);
    if (ret != PSLR_OK) {
        fprintf(stderr, "LVTEST: direct buffer open failed: %d\n", ret);
        return ret;
    }

    expected = pslr_buffer_get_size(cam);
    fprintf(stderr, "LVTEST: buffer opened, expected %u bytes\n", expected);

    fd = open(filename, FILE_ACCESS, 0664);
    if (fd < 0) {
        perror("open output");
        pslr_buffer_close(cam);
        return -1;
    }

    while (1) {
        uint32_t n = pslr_buffer_read(cam, buf, sizeof(buf));
        if (n == 0) {
            break;
        }
#ifdef WIN32
        int w = write(fd, buf, n);
#else
        ssize_t w = write(fd, buf, n);
#endif
        if (w < 0 || (uint32_t)w != n) {
            perror("write output");
            close(fd);
            pslr_buffer_close(cam);
            return -1;
        }
        total += n;
        fprintf(stderr, "\rLVTEST: downloaded %u / %u bytes", total, expected);
        fflush(stderr);
    }
    fprintf(stderr, "\n");

    close(fd);
    pslr_buffer_close(cam);

    if (total == 0 || (expected != 0 && total != expected)) {
        fprintf(stderr, "LVTEST: download incomplete (%u / %u)\n", total, expected);
        return -1;
    }

    fprintf(stderr, "LVTEST: saved %s (%u bytes)\n", filename, total);
    return PSLR_OK;
}

int main(int argc, char **argv) {
    char *device = NULL;
    const char *output = "lvtest.pef";
    bool debug = false;
    bool poll = false;
    bool delete_after = false;
    pslr_handle_t cam = NULL;
    int i;
    int ret = 1;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--device") && i + 1 < argc) {
            device = argv[++i];
        } else if (!strcmp(argv[i], "--output") && i + 1 < argc) {
            output = argv[++i];
        } else if (!strcmp(argv[i], "--debug")) {
            debug = true;
        } else if (!strcmp(argv[i], "--poll")) {
            poll = true;
        } else if (!strcmp(argv[i], "--delete")) {
            delete_after = true;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    pslr_set_verbosity(debug ? PSLR_DEBUG : PSLR_ERROR);

    fprintf(stderr, "LVTEST: experimental passive Live View transfer\n");
    fprintf(stderr, "LVTEST: do NOT use the normal GUI at the same time\n");
    fprintf(stderr, "LVTEST: opening camera%s%s\n", device ? " on device " : "", device ? device : "");

    cam = pslr_init(NULL, device);
    if (!cam) {
        fprintf(stderr, "LVTEST: camera not found. Check MSC mode and drive letter.\n");
        return 3;
    }

    if (pslr_connect(cam) != PSLR_OK) {
        fprintf(stderr, "LVTEST: pslr_connect failed. LV may be incompatible even with the minimal path.\n");
        pslr_shutdown(cam);
        return 4;
    }

    fprintf(stderr, "LVTEST: Connected: %s\n", pslr_get_camera_name(cam));
    fprintf(stderr, "LVTEST: normal status/settings initialization is intentionally SKIPPED.\n");

    if (!poll) {
        fprintf(stderr, "\nTrigger ONE photo with the IR remote.\n");
        fprintf(stderr, "After the shutter cycle and HDMI Live View returns, press Enter here...\n");
        fflush(stderr);
        (void)getchar();
        /* Give the camera a short moment to finish producing the RAW buffer. */
        sleep_sec(0.8);
    } else {
        uint32_t mask = 0;
        int loops = 0;
        fprintf(stderr, "LVTEST: polling lightweight buffer mask; waiting for buffer 0...\n");
        while (1) {
            int r = pslr_get_buffer_mask_direct(cam, &mask);
            if (r != PSLR_OK) {
                fprintf(stderr, "LVTEST: buffer-mask poll failed: %d\n", r);
                goto out;
            }
            if (debug || (loops % 10) == 0) {
                fprintf(stderr, "LVTEST: mask=0x%08X\n", mask);
            }
            if (mask & 0x1) {
                break;
            }
            sleep_sec(0.1);
            loops++;
        }
        fprintf(stderr, "LVTEST: buffer 0 is ready.\n");
    }

    /* Retry direct-open briefly in case Enter was pressed just before RAW finalized. */
    for (i = 0; i < 15; i++) {
        ret = save_pef_direct(cam, output);
        if (ret == PSLR_OK) {
            break;
        }
        fprintf(stderr, "LVTEST: buffer not ready/open failed; retry %d/15...\n", i + 1);
        sleep_sec(0.2);
    }

    if (ret == PSLR_OK && delete_after) {
        int dr = pslr_delete_buffer(cam, 0);
        fprintf(stderr, "LVTEST: delete buffer result = %d\n", dr);
    } else if (ret == PSLR_OK) {
        fprintf(stderr, "LVTEST: buffer was NOT deleted (safer first test). Power-cycle or unplug USB after test if needed.\n");
    }

out:
    /* Important: do not call pslr_disconnect() in this experiment; it issues
       extra camera-control commands that can conflict with Live View. Closing
       the OS handle is sufficient for this one-shot diagnostic. */
    pslr_shutdown(cam);

    if (ret == PSLR_OK) {
        fprintf(stderr, "LVTEST: SUCCESS. Verify that the PEF opens normally and that HDMI LV survived.\n");
        return 0;
    }
    fprintf(stderr, "LVTEST: FAILED. No camera firmware changes were made.\n");
    return 5;
}
