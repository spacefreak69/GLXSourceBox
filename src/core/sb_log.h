#ifndef SB_LOG_H
#define SB_LOG_H

//logging messages

// opens the file or creates it if it doesnt exist
int sb_log_init(void);

// returns 1 when the log is already opened
int sb_log_is_open(void);

// return absolute path of the log file. empty string on failure
const char *sb_log_path(void);

// writes a message with a timestamp, auto appends a \n character because i am too lazy to write that all the time
void sb_log_printf(const char *fmt, ...);

// write line with no timestamp, used for first message
void sb_log_raw(const char *fmt, ...);

// flush to disk and close the file
void sb_log_close(void);

#endif /* SB_LOG_H */