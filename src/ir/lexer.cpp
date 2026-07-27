// Copyright 2026 Mario Vinciguerra
// SPDX-License-Identifier: Apache-2.0

#include "forge/ir/lexer.hpp"
#include <cctype>

namespace forge::ir {
LexResult lex(std::string_view source) {
    LexResult out;
    std::size_t i = 0;
    auto push = [&](TokenKind kind, std::size_t begin, std::size_t end) {
        out.tokens.push_back({kind, std::string(source.substr(begin, end-begin)), {begin,end}});
    };
    while (i < source.size()) {
        if (std::isspace(static_cast<unsigned char>(source[i]))) { ++i; continue; }
        if (source[i]=='/' && i+1<source.size() && source[i+1]=='/') {
            i += 2; while (i<source.size() && source[i]!='\n') ++i; continue;
        }
        const auto begin=i;
        if (source[i]=='-' && i+1<source.size() && source[i+1]=='>') { i+=2; push(TokenKind::arrow,begin,i); continue; }
        if (source[i]=='"') {
            ++i;
            bool escaped = false;
            while (i < source.size()) {
                if (!escaped && source[i] == '"') { ++i; break; }
                if (!escaped && source[i] == '\\') escaped = true;
                else escaped = false;
                ++i;
            }
            if (i > source.size() || source[i - 1] != '"')
                out.diagnostics.push_back({DiagnosticSeverity::error,"unterminated string literal",{begin,i}});
            push(TokenKind::string, begin, i); continue;
        }
        if (source[i]=='@' || source[i]=='%') {
            const bool value=source[i]=='%'; ++i;
            while (i<source.size() && (std::isalnum(static_cast<unsigned char>(source[i]))||source[i]=='_'||source[i]=='.')) ++i;
            if (i==begin+1) out.diagnostics.push_back({DiagnosticSeverity::error,"expected name after sigil",{begin,i}});
            push(value?TokenKind::value:TokenKind::symbol,begin,i); continue;
        }
        if (std::isalpha(static_cast<unsigned char>(source[i])) || source[i]=='_') {
            ++i; while (i<source.size() && (std::isalnum(static_cast<unsigned char>(source[i]))||source[i]=='_'||source[i]=='.')) ++i;
            push(TokenKind::identifier,begin,i); continue;
        }
        if (std::isdigit(static_cast<unsigned char>(source[i])) || (source[i]=='-'&&i+1<source.size()&&std::isdigit(static_cast<unsigned char>(source[i+1])))) {
            ++i;
            while (i<source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) ++i;
            if (i < source.size() && source[i] == '.') {
                ++i; while (i<source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) ++i;
            }
            if (i < source.size() && (source[i] == 'e' || source[i] == 'E')) {
                ++i; if (i < source.size() && (source[i] == '+' || source[i] == '-')) ++i;
                while (i<source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) ++i;
            }
            push(TokenKind::integer,begin,i); continue;
        }
        if (std::string_view("{}[]():,=").find(source[i]) != std::string_view::npos) { ++i; push(TokenKind::punctuation,begin,i); continue; }
        out.diagnostics.push_back({DiagnosticSeverity::error,"unexpected character",{begin,begin+1}}); ++i;
    }
    out.tokens.push_back({TokenKind::end,"",{source.size(),source.size()}});
    return out;
}
}
