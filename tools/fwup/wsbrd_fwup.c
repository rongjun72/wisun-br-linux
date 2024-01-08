/*
 * Copyright (c) 2023 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1].  This software is distributed to you in
 * Object Code format and/or Source Code format and is governed by the sections
 * of the MSLA applicable to Object Code, Source Code and Modified Open Source
 * Code. By using this software, you agree to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include "common/log.h"
#include "common/bus_uart.h"
#include "common/os_types.h"
#include "common/spinel_buffer.h"
#include "common/spinel_defs.h"
#include "common/iobuf.h"
#include "common/utils.h"
#include "common/bits.h"


struct commandline_args {
    int rcp_uart_baudrate;
    char rcp_uart_dev[PATH_MAX];
    char gbl_file_path[PATH_MAX];
};

void print_help(FILE *stream, int exit_code)
{
    fprintf(stream, "Usage:\n");
    fprintf(stream, "  wsbrd-fwup [OPTIONS] GBL_FILE\n");
    fprintf(stream, "\n");
    fprintf(stream, "Flash the Wi-SUN Radio Co-Processor (RCP) device with GBL_FILE. GBL_FILE must be\n");
    fprintf(stream, "a Gecko Bootloader file (see Silicon Labs document UG266 for more information\n");
    fprintf(stream, "how to generate that file).\n");
    fprintf(stream, "\n");
    fprintf(stream, "wsbrd-fwup expects the device to have the \"bootloader-uart-xmodem\" component.\n");
    fprintf(stream, "Otherwise the update fails without affecting the device.\n");
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -u, --uart=DEVICE  Use UART bus (default: /dev/ttyACM0)\n");
    fprintf(stream, "  -B, --baurate=RATE Configure UART with this baudrate (default: 115200)\n");
    exit(exit_code);
}

void parse_commandline(struct commandline_args *cmd, int argc, char *argv[])
{
    struct stat st;
    const char *opts_short = "u:B:h";
    static const struct option opts_long[] = {
            { "uart",     required_argument, 0,  'u' },
            { "baudrate", required_argument, 0,  'B' },
            { "help",     no_argument,       0,  'h' },
            { 0,          0,                 0,   0  }
    };
    int opt, ret;

    cmd->rcp_uart_baudrate = 115200;
    strcpy(cmd->rcp_uart_dev, "/dev/ttyACM0");
    while ((opt = getopt_long(argc, argv, opts_short, opts_long, NULL)) != -1) {
        switch (opt) {
            case 'u':
                strcpy(cmd->rcp_uart_dev, optarg);
                break;
            case 'B':
                cmd->rcp_uart_baudrate = strtol(optarg, NULL, 10);
                break;
            case 'h':
                print_help(stdout, 0);
                break;
            case '?':
                print_help(stderr, 1);
                break;
            default:
                break;
        }
    }
    if (optind >= argc)
        FATAL(1, "expected argument: GBL_FILE");
    if (optind + 1 < argc)
        FATAL(1, "unexpected argument: %s", argv[optind + 1]);
    if(strlen(argv[optind]) >= PATH_MAX)
        FATAL(1, "argument too long: gbl_file_path exceeds PATH_MAX");
    strcpy(cmd->gbl_file_path, argv[optind]);
    ret = stat(cmd->gbl_file_path, &st);
    FATAL_ON(ret, 1, "%s does not exists", cmd->gbl_file_path);
}

static size_t read_data(struct os_ctxt *ctxt, uint8_t *buf, int buf_len)
{
    int len, ret;
    struct pollfd pollfd = {
        .fd = ctxt->trig_fd,
        .events = POLLIN,
    };

    do {
        if (!ctxt->uart_next_frame_ready) {
            ret = poll(&pollfd, 1, 5000);
            if (ret < 0)
                FATAL(2, "poll: %m");
            if (!ret)
                return 0;
        }

        if (pollfd.revents & POLLIN || ctxt->uart_next_frame_ready)
            len = uart_rx(ctxt, buf, buf_len);
    } while (!len);
    return len;
}

static void send_btl_update(struct os_ctxt *ctxt)
{
    struct iobuf_write tx_buf = { };

    spinel_push_u8(&tx_buf, 0);
    spinel_push_uint(&tx_buf, SPINEL_CMD_BOOTLOADER_UPDATE);
    uart_tx(ctxt, tx_buf.data, tx_buf.len);
    iobuf_free(&tx_buf);
}

static void handle_btl_update(struct os_ctxt *ctxt)
{
    int ret;
    char btl_rx_buf[4096];
    char btl_upload_gbl = '1';
    char *btl_str = "Gecko Bootloader";
    struct pollfd pollfd = { .events = POLLIN };

    pollfd.fd = ctxt->trig_fd;
    sleep(1); // wait for rcp to reboot
    ret = poll(&pollfd, 1, 1000);
    if (ret < 0)
        FATAL(2, "poll: %m");
    if (!ret)
        FATAL(1, "poll: SPINEL_CMD_BOOTLOADER_UPDATE is not implemented on this RCP");
    ret = read(ctxt->data_fd, btl_rx_buf, sizeof(btl_rx_buf));
    if (!memmem(btl_rx_buf, ret, btl_str, strlen(btl_str)))
        FATAL(1, "cannot get bootloader banner");
    // option '1' to upload gbl
    write(ctxt->data_fd, &btl_upload_gbl, sizeof(uint8_t));
}

static void handle_btl_run(struct os_ctxt *ctxt)
{
    char btl_run = '2';

    // wait for the Gecko Bootloader banner
    usleep(500000);
    // option '2' to run
    write(ctxt->data_fd, &btl_run, sizeof(uint8_t));
}

static void handle_rcp_reset(struct os_ctxt *ctxt)
{
    int cmd;
    const char *version_fw_str;
    uint32_t rcp_version_api, rcp_version_fw;
    uint8_t buffer[4096] = { };
    struct iobuf_read rx_buf = { };

    ctxt->uart_inhibit_crc_warning = true;
    rx_buf.data = buffer;
    rx_buf.data_size = read_data(ctxt, buffer, sizeof(buffer));
    if (!rx_buf.data_size) {
        INFO("No RCP version received");
        return;
    }

    spinel_pop_u8(&rx_buf);
    cmd = spinel_pop_uint(&rx_buf);
    if (cmd == SPINEL_CMD_NOOP) {
        rx_buf.cnt = 0;
        rx_buf.data_size = read_data(ctxt, buffer, sizeof(buffer));
        spinel_pop_u8(&rx_buf);
        cmd = spinel_pop_uint(&rx_buf);
    }
    if (cmd != SPINEL_CMD_RESET)
        FATAL(1, "unexpected firmware boot sequence");
    rcp_version_api = spinel_pop_u32(&rx_buf);
    rcp_version_fw = spinel_pop_u32(&rx_buf);
    version_fw_str = spinel_pop_str(&rx_buf);

    INFO("Updated to RCP \"%s\" (%d.%d.%d), API %d.%d.%d", version_fw_str,
         FIELD_GET(0xFF000000, rcp_version_fw),
         FIELD_GET(0x00FFFF00, rcp_version_fw),
         FIELD_GET(0x000000FF, rcp_version_fw),
         FIELD_GET(0xFF000000, rcp_version_api),
         FIELD_GET(0x00FFFF00, rcp_version_api),
         FIELD_GET(0x000000FF, rcp_version_api));
}

static void check_if_sx_is_installed()
{
    pid_t pid;
    int wstatus;
    char *sx_args[] = { "sx", "--version", NULL };

    pid = fork();
    if (pid > 0) {
        waitpid(pid, &wstatus, 0);
    } else if (pid == 0) {
        execvp("sx", sx_args);
        FATAL(1, "Command 'sx' (usually provided by package 'lrzsz') is not installed on your system.");
    } else {
        FATAL(2, "fork: %m");
    }

    if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus))
        exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    pid_t pid;
    int ret, wstatus, sxfd;
    struct commandline_args cmdline = { };
    struct os_ctxt ctxt = { };
    char *sx_args[] = { "sx", "-vv", cmdline.gbl_file_path, NULL };

    parse_commandline(&cmdline, argc, argv);
    check_if_sx_is_installed();

    ctxt.data_fd = uart_open(cmdline.rcp_uart_dev, cmdline.rcp_uart_baudrate, false);
    ctxt.trig_fd = ctxt.data_fd;
    FATAL_ON(ctxt.data_fd < 0, 2, "%s: %m", cmdline.rcp_uart_dev);
    send_btl_update(&ctxt);
    handle_btl_update(&ctxt);
    close(ctxt.data_fd);

    pid = fork();
    if (pid > 0) {
        waitpid(pid, &wstatus, 0);
    } else if (pid == 0) {
        sxfd = open(cmdline.rcp_uart_dev, O_RDWR);
        FATAL_ON(sxfd == -1, 2, "open %s: %m", cmdline.rcp_uart_dev);
        ret = dup2(sxfd, STDIN_FILENO);
        FATAL_ON(ret == -1, 2, "dup2: %m");
        ret = dup2(sxfd, STDOUT_FILENO);
        FATAL_ON(ret == -1, 2, "dup2: %m");
        ret = close(sxfd);
        FATAL_ON(ret == -1, 2, "close: %m");
        execvp("sx", sx_args);
        FATAL(2, "execv: %m");
    } else {
        FATAL(2, "fork: %m");
    }

    if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus))
        FATAL(1, "xmodem transfer failed");

    ctxt.data_fd = uart_open(cmdline.rcp_uart_dev, cmdline.rcp_uart_baudrate, false);
    ctxt.trig_fd = ctxt.data_fd;
    FATAL_ON(ctxt.data_fd < 0, 2, "%s: %m", cmdline.rcp_uart_dev);
    handle_btl_run(&ctxt);
    handle_rcp_reset(&ctxt);
    close(ctxt.data_fd);

    return 0;
}
