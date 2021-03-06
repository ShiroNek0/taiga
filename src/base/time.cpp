/*
** Taiga
** Copyright (C) 2010-2014, Eren Okka
** 
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cmath>
#include <regex>

#include "string.h"
#include "time.h"

Date::Date()
    : year(0), month(0), day(0) {
}

Date::Date(const std::wstring& date)
    : year(0), month(0), day(0) {
  // Convert from YYYY-MM-DD
  if (date.length() >= 10) {
    year = ToInt(date.substr(0, 4));
    month = ToInt(date.substr(5, 2));
    day = ToInt(date.substr(8, 2));
  }
}

Date::Date(unsigned short year, unsigned short month, unsigned short day)
    : year(year), month(month), day(day) {
}

Date& Date::operator = (const Date& date) {
  year = date.year;
  month = date.month;
  day = date.day;

  return *this;
}

int Date::operator - (const Date& date) const {
  return ((year * 365) + (month * 30) + day) -
         ((date.year * 365) + (date.month * 30) + date.day);
}

Date::operator bool() const {
  return year != 0 || month != 0 || day != 0;
}

Date::operator SYSTEMTIME() const {
  SYSTEMTIME st = {0};
  st.wYear = year;
  st.wMonth = month;
  st.wDay = day;

  return st;
}

Date::operator std::wstring() const {
  // Convert to YYYY-MM-DD
  return PadChar(ToWstr(year), '0', 4) + L"-" +
         PadChar(ToWstr(month), '0', 2) + L"-" +
         PadChar(ToWstr(day), '0', 2);
}

base::CompareResult Date::Compare(const Date& date) const {
  if (year != date.year) {
    if (year == 0)
      return base::kGreaterThan;
    if (date.year == 0)
      return base::kLessThan;
    return year < date.year ? base::kLessThan : base::kGreaterThan;
  }

  if (month != date.month) {
    if (month == 0)
      return base::kGreaterThan;
    if (date.month == 0)
      return base::kLessThan;
    return month < date.month ? base::kLessThan : base::kGreaterThan;
  }

  if (day != date.day) {
    if (day == 0)
      return base::kGreaterThan;
    if (date.day == 0)
      return base::kLessThan;
    return day < date.day ? base::kLessThan : base::kGreaterThan;
  }

  return base::kEqualTo;
}

////////////////////////////////////////////////////////////////////////////////

static void NeutralizeTimezone(tm& t) {
  long timezone_difference = 0;
  if (_get_timezone(&timezone_difference) == 0)
    t.tm_sec -= timezone_difference;  // mktime uses the current time zone
}

time_t ConvertIso8601(const std::wstring& datetime) {
  // e.g.
  // "2015-02-20T04:43:50"
  // "2015-02-20T04:43:50.016Z"
  // "2015-02-20T06:43:50.016+02:00"
  static const std::wregex pattern(
      L"(\\d{4})-(\\d{2})-(\\d{2})"
      L"T(\\d{2}):(\\d{2}):(\\d{2})(?:[.,]\\d+)?"
      L"(?:(?:([+-])(\\d{2}):(\\d{2}))|Z)?");

  std::match_results<std::wstring::const_iterator> m;
  time_t result = -1;

  if (std::regex_match(datetime, m, pattern)) {
    tm t = {0};
    t.tm_year = ToInt(m[1].str()) - 1900;
    t.tm_mon = ToInt(m[2].str()) - 1;
    t.tm_mday = ToInt(m[3].str());
    t.tm_hour = ToInt(m[4].str());
    t.tm_min = ToInt(m[5].str());
    t.tm_sec = ToInt(m[6].str());

    if (m[7].matched) {
      int sign = m[7].str() == L"+" ? 1 : -1;
      t.tm_hour += sign * ToInt(m[8].str());
      t.tm_min += sign * ToInt(m[9].str());
    }
    NeutralizeTimezone(t);

    result = mktime(&t);
  }

  return result;
}

time_t ConvertRfc822(const std::wstring& datetime) {
  // See: https://tools.ietf.org/html/rfc822#section-5
  static const std::wregex pattern(
      L"(?:(Mon|Tue|Wed|Thu|Fri|Sat|Sun), )?"
      L"(\\d{1,2}) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) (\\d{2,4}) "
      L"(\\d{2}):(\\d{2})(?::(\\d{2}))? "
      L"(UT|GMT|EST|EDT|CST|CDT|MST|MDT|PST|PDT|[ZAMNY]|[+-]\\d{4})");

  static const std::vector<std::wstring> months{
    L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", 
    L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
  };

  std::match_results<std::wstring::const_iterator> m;
  time_t result = -1;

  if (std::regex_match(datetime, m, pattern)) {
    tm t = {0};

    t.tm_mday = ToInt(m[2].str());
    t.tm_mon = std::distance(months.begin(),
        std::find(months.begin(), months.end(), m[3].str()));
    t.tm_year = ToInt(m[4].str());
    if (t.tm_year > 1900)
      t.tm_year -= 1900;

    t.tm_hour = ToInt(m[5].str());
    t.tm_min = ToInt(m[6].str());
    if (m[7].matched)
      t.tm_sec = ToInt(m[7].str());

    // TODO: Get time zone from m[8]
    NeutralizeTimezone(t);

    result = mktime(&t);
  }

  return result;
}

std::wstring GetRelativeTimeString(time_t unix_time) {
  if (!unix_time)
    return L"Unknown";

  std::tm tm;
  if (localtime_s(&tm, &unix_time)) {
    return L"Unknown";
  }

  std::wstring str;
  auto date_now = GetDate();

  auto str_time = [](std::tm& tm, const char* format) {
    std::string result(100, '\0');
    std::strftime(&result.at(0), result.size(), format, &tm);
    return StrToWstr(result);
  };

  if (1900 + tm.tm_year < date_now.year) {
    str = Date(1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday);  // YYYY-MM-DD
  } else if (tm.tm_mon + 1 < date_now.month) {
    str = str_time(tm, "%B %d");  // January 1
  } else {
    time_t seconds = time(nullptr) - unix_time;
    if (seconds >= 60 * 60 * 24) {
      auto value = std::lround(static_cast<float>(seconds) / (60 * 60 * 24));
      str = ToWstr(value) + (value == 1 ? L" day" : L" days");
    } else if (seconds >= 60 * 60) {
      auto value = std::lround(static_cast<float>(seconds) / (60 * 60));
      str = ToWstr(value) + (value == 1 ? L" hour" : L" hours");
    } else if (seconds >= 60) {
      auto value = std::lround(static_cast<float>(seconds) / 60);
      str = ToWstr(value) + (value == 1 ? L" minute" : L" minutes");
    } else {
      str = L"a moment";
    }
    str += L" ago";
  }

  return str;
}

void GetSystemTime(SYSTEMTIME& st, int utc_offset) {
  // Get current time, expressed in UTC
  GetSystemTime(&st);
  if (utc_offset == 0)
    return;

  // Convert to FILETIME
  FILETIME ft;
  SystemTimeToFileTime(&st, &ft);
  // Convert to ULARGE_INTEGER
  ULARGE_INTEGER ul;
  ul.LowPart = ft.dwLowDateTime;
  ul.HighPart = ft.dwHighDateTime;

  // Apply UTC offset
  ul.QuadPart += static_cast<ULONGLONG>(utc_offset) * 60 * 60 * 10000000;

  // Convert back to SYSTEMTIME
  ft.dwLowDateTime = ul.LowPart;
  ft.dwHighDateTime = ul.HighPart;
  FileTimeToSystemTime(&ft, &st);
}

Date GetDate() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  return Date(st.wYear, st.wMonth, st.wDay);
}

std::wstring GetTime(LPCWSTR format) {
  WCHAR buff[32];
  GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, format, buff, 32);
  return buff;
}

Date GetDateJapan() {
  SYSTEMTIME st_jst;
  GetSystemTime(st_jst, 9);  // JST is UTC+09
  return Date(st_jst.wYear, st_jst.wMonth, st_jst.wDay);
}

std::wstring GetTimeJapan(LPCWSTR format) {
  WCHAR buff[32];
  SYSTEMTIME st_jst;
  GetSystemTime(st_jst, 9);  // JST is UTC+09
  GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, &st_jst, format, buff, 32);
  return buff;
}

std::wstring ToDateString(time_t seconds) {
  time_t days, hours, minutes;
  std::wstring date;

  if (seconds > 0) {
    #define CALC_TIME(x, y) x = seconds / (y); seconds = seconds % (y);
    CALC_TIME(days, 60 * 60 * 24);
    CALC_TIME(hours, 60 * 60);
    CALC_TIME(minutes, 60);
    #undef CALC_TIME
    date.clear();
    #define ADD_TIME(x, y) \
      if (x > 0) { \
        if (!date.empty()) date += L" "; \
        date += ToWstr(x) + y; \
        if (x > 1) date += L"s"; \
      }
    ADD_TIME(days, L" day");
    ADD_TIME(hours, L" hour");
    ADD_TIME(minutes, L" minute");
    ADD_TIME(seconds, L" second");
    #undef ADD_TIME
  }

  return date;
}

unsigned int ToDayCount(const Date& date) {
  return (date.year * 365) + (date.month * 30) + date.day;
}

std::wstring ToTimeString(int seconds) {
  int hours = seconds / 3600;
  seconds = seconds % 3600;
  int minutes = seconds / 60;
  seconds = seconds % 60;

  #define TWO_DIGIT(x) (x >= 10 ? ToWstr(x) : L"0" + ToWstr(x))
  return (hours > 0 ? TWO_DIGIT(hours) + L":" : L"") +
         TWO_DIGIT(minutes) + L":" + TWO_DIGIT(seconds);
  #undef TWO_DIGIT
}

const Date& EmptyDate() {
  static const Date date;
  return date;
}