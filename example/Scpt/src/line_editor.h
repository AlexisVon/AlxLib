// Copyright (c) 2026 AlexisVon

#pragma once
#ifndef _WIN32

#    include <string>
#    include <termios.h>
#    include <vector>

class LineEditor {
public:
    LineEditor();
    ~LineEditor();

    bool readline(std::string& out);

private:
    bool read_char(char& c);
    void handle_escape();
    void refresh();
    void move_cursor(int delta);
    void set_cursor(size_t pos);
    void history_prev();
    void history_next();

    termios m_orig;
    std::string m_buf;
    size_t m_pos = 0;

    std::vector<std::string> m_hist;
    int m_hist_idx = -1;
    std::string m_saved;
};

#endif
