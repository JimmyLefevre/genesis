
static void loglnprint(Log *log, string s) {
    usize print_length = s.length + 1; // Appending \n
    if((log->temporary_buffer.length + print_length) < log->temporary_buffer_size) {
        u8 *append_at = log->temporary_buffer.data + log->temporary_buffer.length;
        mem_copy(s.data, append_at, s.length);
        append_at[s.length] = '\n';
        append_at[print_length] = '\0';
        log->temporary_buffer.length += print_length;
    }
}

static void logprint(Log *log, string s) {
#if 1
    append_string(&log->temporary_buffer, s, log->temporary_buffer_size);
#else
    // +1 to make sure we can zero-terminate.
    if((log->temporary_buffer.length + s.length + 1) < log->temporary_buffer_size) {
        u8 *append_at = log->temporary_buffer.data + log->temporary_buffer.length;
        mem_copy(s.data, append_at, s.length);
        
        log->temporary_buffer.length += s.length;
    }
#endif
}

static void logprint(Log *log, char *format, ...) {
    char *varargs;
    BEGIN_VARARG(varargs, format);
    usize currently_printed = log->temporary_buffer.length;
    usize printed = _print((char *)log->temporary_buffer.data + currently_printed, log->temporary_buffer_size - currently_printed, format, varargs);
    log->temporary_buffer.length += printed;
}

static void headsup(Log *log, string s) {
    if((s.length + 1) < log->heads_up_buffer_size) {
        mem_copy(s.data, log->heads_up_buffer.data, s.length);
        log->heads_up_buffer.length = s.length;
        log->heads_up_alpha = 1.0f;
    }
}

static void headsup(Log *log, char *format, ...) {
    char *varargs;
    BEGIN_VARARG(varargs, format);
    log->heads_up_buffer.length = _print((char *)log->heads_up_buffer.data, log->heads_up_buffer_size, format, varargs);
    log->heads_up_alpha = 1.0f;
}

static void flush_log_to_standard_output(Log *log) {
    os_platform.print(log->temporary_buffer);
    log->temporary_buffer.length = 0;
}
