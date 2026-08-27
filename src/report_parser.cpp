#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct BatteryEvent {
    std::string date;
    std::string time;
    std::string state;
    std::string source;
    std::string percent;
    std::string mwh;
};

// Reads an entire file into a single string
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Removes leading/trailing whitespace and newlines
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

// Finds text between a start marker and an end marker, starting search at 'pos'.
// Advances 'pos' past the match so repeated calls move forward through the string.
std::string extractBetween(const std::string& text, const std::string& startTag,
                            const std::string& endTag, size_t& pos) {
    size_t startPos = text.find(startTag, pos);
    if (startPos == std::string::npos) return "";
    startPos += startTag.length();
    size_t endPos = text.find(endTag, startPos);
    if (endPos == std::string::npos) return "";
    pos = endPos + endTag.length();
    return text.substr(startPos, endPos - startPos);
}

std::vector<BatteryEvent> parseRecentUsage(const std::string& html) {
    std::vector<BatteryEvent> events;

    size_t sectionStart = html.find("Recent usage");
    if (sectionStart == std::string::npos) {
        std::cout << "Could not find 'Recent usage' section.\n";
        return events;
    }

    size_t theadEnd = html.find("</thead>", sectionStart);
    if (theadEnd == std::string::npos) return events;

    size_t tableEnd = html.find("</table>", theadEnd);
    size_t pos = theadEnd;
    std::string lastDate;

    while (true) {
        size_t rowStart = html.find("<tr", pos);
        if (rowStart == std::string::npos) break;
        if (tableEnd != std::string::npos && rowStart > tableEnd) break;

        size_t rowEnd = html.find("</tr>", rowStart);
        if (rowEnd == std::string::npos) break;

        std::string row = html.substr(rowStart, rowEnd - rowStart);
        pos = rowEnd + 5;

        BatteryEvent ev;

        size_t p = 0;
        std::string dateTimeBlock = extractBetween(row, "class=\"dateTime\">", "</td>", p);
        size_t dp = 0;
        std::string dateStr = trim(extractBetween(dateTimeBlock, "class=\"date\">", "</span>", dp));
        std::string timeStr = trim(extractBetween(dateTimeBlock, "class=\"time\">", "</span>", dp));

        if (!dateStr.empty()) lastDate = dateStr;
        ev.date = lastDate;
        ev.time = timeStr;

        size_t p2 = 0;
        ev.state = trim(extractBetween(row, "class=\"state\">", "</td>", p2));

        size_t p3 = 0;
        ev.source = trim(extractBetween(row, "class=\"acdc\">", "</td>", p3));

        size_t p4 = 0;
        ev.percent = trim(extractBetween(row, "class=\"percent\">", "</td>", p4));

        size_t p5 = 0;
        ev.mwh = trim(extractBetween(row, "class=\"mw\">", "</td>", p5));

        if (!ev.time.empty()) {
            events.push_back(ev);
        }
    }

    return events;
}

int main() {
    std::string html = readFile("report.html");
    if (html.empty()) {
        std::cout << "Could not read report.html - make sure it's in this folder.\n";
        return 1;
    }

    std::vector<BatteryEvent> events = parseRecentUsage(html);
    std::cout << "Parsed " << events.size() << " events.\n\n";

    // Print first 10 as a sanity check
    for (size_t i = 0; i < events.size() && i < 10; i++) {
        std::cout << events[i].date << " " << events[i].time
                   << " | " << events[i].state
                   << " | " << events[i].source
                   << " | " << events[i].percent
                   << " | " << events[i].mwh << "\n";
    }

    // Write full data to CSV
    std::ofstream csv("data/battery_report_history.csv");
    csv << "date,time,state,source,percent,mwh\n";
    for (auto& ev : events) {
        csv << ev.date << "," << ev.time << "," << ev.state << ","
            << ev.source << "," << ev.percent << "," << ev.mwh << "\n";
    }
    csv.close();

    std::cout << "\nWrote data/battery_report_history.csv\n";
    return 0;
}