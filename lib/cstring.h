/*
Copyright 2013-present Barefoot Networks, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef LIB_CSTRING_H_
#define LIB_CSTRING_H_

#include <cstddef>
#include <cstring>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "hash.h"

/**
 * A cstring is a reference to a zero-terminated, immutable, interned string.
 * The cstring object *itself* is not immutable; you can reassign it as
 * required, and it provides a mutable interface that copies the underlying
 * immutable string as needed.
 *
 * Compared to std::string, these are the benefits that cstring provides:
 *   - Copying and assignment are cheap. Since cstring only deals with immutable
 *     strings, these operations only involve pointer assignment.
 *   - Comparing cstrings for equality is cheap; interning makes it possible to
 *     test for equality using a simple pointer comparison.
 *   - The immutability of the underlying strings means that it's always safe to
 *     change a cstring, even if there are other references to it elsewhere.
 *   - The API offers a number of handy helper methods that aren't available on
 *     std::string.
 *
 * On the other hand, these are the disadvantages of using cstring:
 *   - Because cstring deals with immutable strings, any modification requires
 *     that the complete string be copied.
 *   - Interning has an initial cost: converting a const char*, a
 *     std::string, or a std::stringstream to a cstring requires copying it.
 *     Currently, this happens every time you perform the conversion, whether
 *     the string is already interned or not, unless the source is an
 *     std::string.
 *   - Interned strings can never be freed, so they'll stick around for the
 *     lifetime of the program.
 *   - The string interning cstring performs is currently not threadsafe, so you
 *     can't safely use cstrings off the main thread.
 *
 * Given these tradeoffs, the general rule of thumb to follow is that you should
 * try to convert strings to cstrings early and keep them in that form. That
 * way, you benefit from cheaper copying, assignment, and equality testing in as
 * many places as possible, and you avoid paying the cost of repeated
 * conversion.
 *
 * However, when you're building or mutating a string, you should use
 * std::string. Convert to a cstring only when the string is in its final form.
 * This ensures that you don't pay the time and space cost of interning every
 * intermediate version of the string.
 *
 * Note that cstring has implicit conversions to and from other string types.
 * This is convenient, but in performance-sensitive code it's good to be aware
 * that mixing the two types of strings can trigger a lot of implicit copies.
 */

namespace P4 {
class cstring;
}  // namespace P4

namespace P4::literals {
inline cstring operator""_cs(const char *str, std::size_t len);
}

namespace P4 {
class cstring {
    const char *str = nullptr;

 public:
    cstring() = default;

    cstring(std::nullptr_t) {}  // NOLINT(runtime/explicit)

    // Copy and assignment from other kinds of strings

    // Owner of string is someone else, but we know size of string.
    // Do not use if possible, this is linear time operation if string
    // not exists in table, because the underlying string must be copied.
    cstring(const char *string, std::size_t length) {  // NOLINT(runtime/explicit)
        if (string != nullptr) {
            construct_from_shared(string, length);
        }
    }

    // Owner of string is someone else, we do not know size of string.
    // Do not use if possible, this is linear time operation if string
    // not exists in table, because the underlying string must be copied.
    explicit cstring(const char *string) {
        if (string != nullptr) {
            construct_from_shared(string, std::strlen(string));
        }
    }

    // construct cstring from std::string. Do not use if possible, this is linear
    // time operation if string not exists in table, because the underlying string must be copied.
    cstring(const std::string &string) {  // NOLINT(runtime/explicit)
        construct_from_shared(string.data(), string.length());
    }

    // construct cstring from std::string_view. Do not use if possible, this is linear
    // time operation if string not exists in table, because the underlying string must be copied.
    explicit cstring(std::string_view string) {
        construct_from_shared(string.data(), string.length());
    }

    // TODO (DanilLutsenko): Make special case for r-value std::string?

    // Just helper function, for lazies, who do not like to write .str()
    // Do not use it, implicit std::string construction with implicit overhead
    // TODO (DanilLutsenko): Remove it?
    cstring(const std::stringstream &stream)  // NOLINT(runtime/explicit)
        : cstring(stream.str()) {}

    // TODO (DanilLutsenko): Construct from StringRef?

    // String was created outside and cstring is unique owner of it.
    // cstring will control lifetime of passed object
    static cstring own(const char *string, std::size_t length) {
        if (string == nullptr) {
            return {};
        }

        cstring result;
        result.construct_from_unique(string, length);
        return result;
    }

    // construct cstring wrapper for literal
    template <typename T, std::size_t N,
              typename = typename std::enable_if<std::is_same<T, const char>::value>::type>
    static cstring literal(T (&string)[N]) {  // NOLINT(runtime/explicit)
        cstring result;
        result.construct_from_literal(string, N - 1 /* String length without null terminator */);
        return result;
    }

    /// @return true if a given string is interned (contained in cstring cache)
    static bool is_cached(std::string_view s);
    /// @return corresponding cstring if it was interned, null cstring otherwise
    static cstring get_cached(std::string_view s);

 private:
    // passed string is shared, we not unique owners
    void construct_from_shared(const char *string, std::size_t length);

    // we are unique owners of passed string
    void construct_from_unique(const char *string, std::size_t length);

    // string is literal
    void construct_from_literal(const char *string, std::size_t length);

    friend cstring P4::literals::operator""_cs(const char *str, std::size_t len);

 public:
    /// @return a version of the string where all necessary characters
    /// are properly escaped to make this into a json string (without
    /// the enclosing quotes).
    cstring escapeJson() const;

    template <typename Iter>
    cstring(Iter begin, Iter end) {
        *this = cstring(std::string(begin, end));
    }

    char get(unsigned index) const { return (index < size()) ? str[index] : 0; }
    const char *c_str() const { return str; }
    explicit operator const char *() const { return str; }

    std::string string() const { return str ? std::string(str) : std::string(""); }
    explicit operator std::string() const { return string(); }

    std::string_view string_view() const {
        return str ? std::string_view(str) : std::string_view("");
    }
    operator std::string_view() const { return string_view(); }

    // Size tests. Constant time except for size(), which is linear time.
    size_t size() const {
        // TODO (DanilLutsenko): We store size of string in table on object construction,
        // compiler cannot optimize strlen if str not points to string literal.
        // Probably better fetch size from table or store it to cstring on construction.
        return str ? strlen(str) : 0;
    }
    bool isNull() const { return str == nullptr; }
    bool isNullOrEmpty() const { return str == nullptr ? true : str[0] == 0; }
    explicit operator bool() const { return str; }

    // iterate over characters
    const char *begin() const { return str; }
    const char *end() const { return str ? str + strlen(str) : str; }

    // Search for characters. Linear time.
    const char *find(int c) const { return str ? strchr(str, c) : nullptr; }
    const char *findlast(int c) const { return str ? strrchr(str, c) : str; }

    // Search for substring
    const char *find(const char *s) const { return str ? strstr(str, s) : nullptr; }

    // Equality tests with other cstrings. Constant time.
    bool operator==(cstring a) const { return str == a.str; }
    bool operator!=(cstring a) const { return str != a.str; }

    bool operator==(std::nullptr_t) const { return str == nullptr; }
    bool operator!=(std::nullptr_t) const { return str != nullptr; }

    // Other comparisons and tests. Linear time.
    bool operator==(const char *a) const { return str ? a && !strcmp(str, a) : !a; }
    bool operator!=(const char *a) const { return str ? !a || !!strcmp(str, a) : !!a; }
    bool operator<(cstring a) const { return *this < a.str; }
    bool operator<(const char *a) const { return str ? a && strcmp(str, a) < 0 : !!a; }
    bool operator<=(cstring a) const { return *this <= a.str; }
    bool operator<=(const char *a) const { return str ? a && strcmp(str, a) <= 0 : true; }
    bool operator>(cstring a) const { return *this > a.str; }
    bool operator>(const char *a) const { return str ? !a || strcmp(str, a) > 0 : false; }
    bool operator>=(cstring a) const { return *this >= a.str; }
    bool operator>=(const char *a) const { return str ? !a || strcmp(str, a) >= 0 : !a; }
    bool operator==(std::string_view a) const { return str ? a.compare(str) == 0 : a.empty(); }
    bool operator!=(std::string_view a) const { return str ? a.compare(str) != 0 : !a.empty(); }

    bool operator==(const std::string &a) const { return *this == a.c_str(); }
    bool operator!=(const std::string &a) const { return *this != a.c_str(); }
    bool operator<(const std::string &a) const { return *this < a.c_str(); }
    bool operator<=(const std::string &a) const { return *this <= a.c_str(); }
    bool operator>(const std::string &a) const { return *this > a.c_str(); }
    bool operator>=(const std::string &a) const { return *this >= a.c_str(); }

    bool startsWith(std::string_view prefix) const;
    bool endsWith(std::string_view suffix) const;

    // FIXME (DanilLutsenko): We really need mutations for immutable string?
    // Probably better do transformation in std::string-like containter and
    // then place result to cstring if needed.

    // Mutation operations. These are linear time and always trigger a copy,
    // since the underlying string is immutable. (Note that this is true even
    // for substr(); cstrings are always null-terminated, so a copy is
    // required.)
    cstring operator+=(cstring a);
    cstring operator+=(const char *a);
    cstring operator+=(std::string a);
    cstring operator+=(char a);

    cstring before(const char *at) const;
    cstring substr(size_t start) const {
        return (start >= size()) ? cstring::literal("") : substr(start, size() - start);
    }
    cstring substr(size_t start, size_t length) const;
    cstring replace(char find, char replace) const;
    cstring replace(std::string_view find, std::string_view replace) const;
    cstring exceptLast(size_t count) { return substr(0, size() - count); }

    // trim leading and trailing whitespace (or other)
    cstring trim(const char *ws = " \t\r\n") const;

    // Useful singletons.
    static cstring newline;
    static cstring empty;

    // Static factory functions.
    template <typename T>
    static cstring to_cstring(const T &t) {
        std::stringstream ss;
        ss << t;
        return cstring(ss.str());
    }
    template <typename Iterator>
    static cstring join(Iterator begin, Iterator end, const char *delim = ", ") {
        std::stringstream ss;
        for (auto current = begin; current != end; ++current) {
            if (begin != current) ss << delim;
            ss << *current;
        }
        return cstring(ss.str());
    }
    template <class T>
    static cstring make_unique(const T &inuse, cstring base, char sep = '.');
    template <class T>
    static cstring make_unique(const T &inuse, cstring base, int &counter, char sep = '.');

    /// @return the total size in bytes of all interned strings. @count is set
    /// to the total number of interned strings.
    static size_t cache_size(size_t &count);

    /// Convert the cstring to uppercase.
    cstring toUpper() const;
    /// Convert the cstring to lowercase.
    cstring toLower() const;
    /// Capitalize the first symbol.
    cstring capitalize() const;
    /// Append this many spaces after each newline (and before the first string).
    cstring indent(size_t amount) const;

    /// Helper to simplify usage of cstring in Abseil functions (e.g. StrCat / StrFormat, etc.)
    /// without explicit string_view conversion.
    template <typename Sink>
    friend void AbslStringify(Sink &sink, cstring s) {
        sink.Append(s.string_view());
    }
};

inline bool operator==(const char *a, cstring b) { return b == a; }
inline bool operator!=(const char *a, cstring b) { return b != a; }
inline bool operator==(const std::string &a, cstring b) { return b == a; }
inline bool operator!=(const std::string &a, cstring b) { return b != a; }

inline std::string operator+(cstring a, cstring b) {
    std::string rv(a);
    rv += b;
    return rv;
}
inline std::string operator+(cstring a, const char *b) {
    std::string rv(a);
    rv += b;
    return rv;
}
inline std::string operator+(cstring a, const std::string &b) {
    std::string rv(a);
    rv += b;
    return rv;
}
inline std::string operator+(cstring a, char b) {
    std::string rv(a);
    rv += b;
    return rv;
}
inline std::string operator+(const char *a, cstring b) {
    std::string rv(a);
    rv += b;
    return rv;
}
inline std::string operator+(std::string a, cstring b) {
    a += b;
    return a;
}
inline std::string operator+(char a, cstring b) {
    std::string rv(1, a);
    rv += b;
    return rv;
}

inline cstring cstring::operator+=(cstring a) {
    *this = cstring(*this + a);
    return *this;
}
inline cstring cstring::operator+=(const char *a) {
    *this = cstring(*this + a);
    return *this;
}
inline cstring cstring::operator+=(std::string a) {
    *this = cstring(*this + a);
    return *this;
}
inline cstring cstring::operator+=(char a) {
    *this = cstring(*this + a);
    return *this;
}

inline std::string &operator+=(std::string &a, cstring b) {
    a.append(b.c_str());
    return a;
}

inline cstring cstring::newline = cstring::literal("\n");
inline cstring cstring::empty = cstring::literal("");

template <class T>
cstring cstring::make_unique(const T &inuse, cstring base, int &counter, char sep) {
    if (!inuse.count(base)) return base;

    char suffix[12];
    cstring rv = base;
    do {
        snprintf(suffix, sizeof(suffix) / sizeof(suffix[0]), "%c%d", sep, counter++);
        rv = cstring(base + (const char *)suffix);
    } while (inuse.count(rv));
    return rv;
}

template <class T>
cstring cstring::make_unique(const T &inuse, cstring base, char sep) {
    int counter = 0;
    return make_unique(inuse, base, counter, sep);
}

inline std::ostream &operator<<(std::ostream &out, cstring s) {
    return out << (s.isNull() ? "<null>" : s.string_view());
}

}  // namespace P4

/// Let's prevent literal clashes. A user wishing to use the literal can do using namespace
/// P4::literals, similarly as they can do using namespace std::literals for the standard once.
namespace P4::literals {

/// A user-provided literal suffix to allow creation of cstring from literals: "foo"_cs.
/// Note that the C++ standard mandates that all user-defined literal suffixes defined outside of
/// the standard library must start with underscore.
inline cstring operator""_cs(const char *str, std::size_t len) {
    cstring result;
    result.construct_from_literal(str, len);
    return result;
}
}  // namespace P4::literals

namespace std {
template <>
struct hash<P4::cstring> {
    std::size_t operator()(const P4::cstring &c) const {
        // cstrings are internalized, therefore their addresses are unique; we
        // can just use their address to produce hash.
        return P4::Util::Hash{}(c.c_str());
    }
};
}  // namespace std

namespace P4::Util {
template <>
struct Hasher<cstring> {
    size_t operator()(const cstring &c) const { return Util::Hash{}(c.c_str()); }
};

}  // namespace P4::Util

#endif /* LIB_CSTRING_H_ */
