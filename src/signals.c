/* Copyright © 2015-2016 Przemysław Kusiak
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "signals.h"
#include "partclone.h"
#include "log.h"

sigjmp_buf env;

// this handler will jump to function sigsetjmp() called somewhere before
static void internal_signal_handler(signal_t sig)
{
    /* JUMP .... */ siglongjmp(env, sig);
}

void initialize_handling()
{
    struct sigaction sa = {
        .sa_handler = &internal_signal_handler, 
        .sa_flags = SA_RESTART // Restart the system call, if at all possible
    };

    sigfillset(&sa.sa_mask); // Block every signal during the handler

    struct {
        int sig_num;
        char* sig_char;
    } sigs_to_handle[] = {
        {SIGHUP,  "SIGHUP" },
        {SIGINT,  "SIGINT" },
        {SIGTERM, "SIGTERM"},
        {SIGQUIT, "SIGQUIT"},
        {SIGUSR1, "SIGUSR1"},
        {SIGUSR2, "SIGUSR2"},
        {0}
    };

    for(int i = 0; sigs_to_handle[i].sig_num; i++) {

        if (sigaction(sigs_to_handle[i].sig_num, &sa, NULL) == -1) {
            log_error("Cannot set %s handler.", sigs_to_handle[i].sig_char); 
        } else {
            log_debug("%s handler set.", sigs_to_handle[i].sig_char);
        }
    }
}
