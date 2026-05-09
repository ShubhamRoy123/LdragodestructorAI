#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <thread>
#include "llama.h"
#include "turboquant.hpp"

namespace fs = std::filesystem;

std::string trim_copy(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";

    const size_t last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

bool starts_with_at(const std::string& text, size_t pos, const std::string& prefix) {
    return pos <= text.size() && text.compare(pos, prefix.size(), prefix) == 0;
}

void typewriter_print(const std::string& text, int delay_ms = 12) {
    for (char ch : text) {
        std::cout << ch << std::flush;

        if (ch == '\n' || ch == '\r') {
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
}

std::string lowercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool is_ascii_word_char(unsigned char ch) {
    return std::isalnum(ch) || ch == '_';
}

bool contains_word_ascii(const std::string& text, const std::string& word) {
    if (word.empty()) return false;

    size_t search_from = 0;
    while (true) {
        const size_t pos = text.find(word, search_from);
        if (pos == std::string::npos) return false;

        const bool left_ok = pos == 0 ||
            !is_ascii_word_char(static_cast<unsigned char>(text[pos - 1]));
        const size_t after = pos + word.size();
        const bool right_ok = after >= text.size() ||
            !is_ascii_word_char(static_cast<unsigned char>(text[after]));

        if (left_ok && right_ok) return true;
        search_from = pos + word.size();
    }
}

fs::path detect_workspace_root() {
    fs::path current = fs::absolute(fs::current_path());

    while (!current.empty()) {
        if (fs::exists(current / "CMakeLists.txt") &&
            fs::exists(current / "src" / "main.cpp")) {
            return current;
        }

        if (current == current.root_path()) break;
        current = current.parent_path();
    }

    return fs::absolute(fs::current_path());
}

fs::path resolve_agent_path(const std::string& filename) {
    fs::path path(trim_copy(filename));
    if (path.is_absolute()) return path.lexically_normal();
    return (fs::current_path() / path).lexically_normal();
}

bool is_safe_auto_open_extension(const fs::path& path) {
    const std::string ext = lowercase_ascii(path.extension().string());
    const std::vector<std::string> safe_extensions = {
        ".html", ".htm", ".txt", ".md", ".css", ".js", ".json",
        ".cpp", ".c", ".h", ".hpp", ".py"
    };

    return std::find(safe_extensions.begin(), safe_extensions.end(), ext) != safe_extensions.end();
}

fs::path choose_primary_created_file(const std::vector<fs::path>& created_paths) {
    for (const auto& path : created_paths) {
        const std::string ext = lowercase_ascii(path.extension().string());
        if (ext == ".html" || ext == ".htm") {
            return path;
        }
    }

    for (const auto& path : created_paths) {
        if (is_safe_auto_open_extension(path)) {
            return path;
        }
    }

    return {};
}

std::string quote_shell_arg(const std::string& value) {
    std::string quoted = "\"";
    for (char ch : value) {
        if (ch == '"') quoted += '\\';
        quoted += ch;
    }
    quoted += "\"";
    return quoted;
}

bool is_command_available(const std::string& command_name) {
#ifdef _WIN32
    const std::string command = "where " + command_name + " >nul 2>nul";
#else
    const std::string command = "command -v " + command_name + " >/dev/null 2>&1";
#endif

    return std::system(command.c_str()) == 0;
}

std::string env_var(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : "";
}

bool env_flag_enabled(const char* name) {
    const std::string value = lowercase_ascii(env_var(name));
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

std::string find_vscode_command() {
    if (is_command_available("code")) {
        return "code";
    }

#ifdef _WIN32
    if (is_command_available("code.cmd")) {
        return "code.cmd";
    }

    const std::vector<fs::path> candidate_paths = {
        fs::path(env_var("LOCALAPPDATA")) / "Programs" / "Microsoft VS Code" / "Code.exe",
        fs::path(env_var("PROGRAMFILES")) / "Microsoft VS Code" / "Code.exe",
        fs::path(env_var("PROGRAMFILES(X86)")) / "Microsoft VS Code" / "Code.exe"
    };

    for (const auto& candidate_path : candidate_paths) {
        if (!candidate_path.empty() && fs::exists(candidate_path)) {
            return quote_shell_arg(candidate_path.string());
        }
    }
#endif

    return "";
}

bool open_path_in_default_app(const fs::path& path) {
    if (env_flag_enabled("METEO_NO_OPEN")) {
        return false;
    }

    if (path.empty() || !fs::exists(path) || !is_safe_auto_open_extension(path)) {
        return false;
    }

    const std::string quoted_path = quote_shell_arg(path.string());
    std::string command;
    const std::string ext = lowercase_ascii(path.extension().string());
    if (ext == ".html" || ext == ".htm") {
#ifdef _WIN32
        command = "start \"\" \"" + path.string() + "\"";
#elif defined(__APPLE__)
        command = "open \"" + path.string() + "\"";
#else
        command = "xdg-open \"" + path.string() + "\"";
#endif
        std::cout << "[Open] Browser Live: " << command << std::endl;
        if (std::system(command.c_str()) == 0) {
            std::cout << " " << std::endl;
            return true;
        }
    }

    const std::string vscode_command = find_vscode_command();
    if (!vscode_command.empty()) {
        command = vscode_command + " --reuse-window --goto " + quoted_path;
        std::cout << "[Open] VSCode fallback: " << command << std::endl;
        if (std::system(command.c_str()) == 0) {   
            std::cout << "[Open] VSCode opened!" << std::endl;
            return true;
        }
    }


#ifdef _WIN32
    command = "explorer /select, \"" + path.string() + "\"";
#elif defined(__APPLE__)
    command = "open " + quoted_path;
#else
    command = "xdg-open " + quoted_path;
#endif
    std::cout << "[Open] Fallback: " << command << std::endl;
    std::system(command.c_str());
    return true;


}

bool write_file_with_visible_typing(const fs::path& output_path, const std::string& content) {
    std::ofstream clear_file(output_path, std::ios::binary | std::ios::trunc);
    if (!clear_file.is_open()) {
        return false;
    }
    clear_file.close();

    open_path_in_default_app(output_path);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::ofstream outfile(output_path, std::ios::binary | std::ios::app);

    if (!outfile.is_open()) {
        return false;
    }


    const int delay_ms = content.size() > 6000 ? 0 : (content.size() > 2500 ? 1 : 2);
    for (char ch : content) {
        outfile.put(ch);
        outfile.flush();

        if (delay_ms > 0 && ch != '\n' && ch != '\r') {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }

    outfile.close();
    return true;
}

/// Click "Go Live" button in VS Code (Live Server/Live Preview)
bool click_go_live_button() {
    const std::string vscode_command = find_vscode_command();
    if (vscode_command.empty()) {
        std::cout << "[Live Server] WARNING: VS Code not found in PATH" << std::endl;
        return false;
    }

    std::string command;
#ifdef _WIN32
    // Use --reuse-window to not open new VS Code, and send the Live Preview command
    command = vscode_command + " --reuse-window --command livePreview.start";
#else
    command = vscode_command + " --reuse-window --command livePreview.start";
#endif

    std::cout << "\n[Live Server] Clicking 'Go Live' button..." << std::endl;
    int result = std::system(command.c_str());
    if (result == 0) {
        std::cout << "[Live Server] SUCCESS: Go Live activated! Browser launching..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        return true;
    } else {
        std::cout << "[Live Server] WARNING: Could not click Go Live (ensure Live Server extension is installed)" << std::endl;
        return false;
    }
}

bool run_file_in_external_terminal(const fs::path& path) {
    if (env_flag_enabled("METEO_NO_OPEN") || env_flag_enabled("METEO_NO_RUN")) {
        return false;
    }

    if (path.empty() || !fs::exists(path)) {
        return false;
    }

    const std::string ext = lowercase_ascii(path.extension().string());
    const std::string quoted_path = quote_shell_arg(path.string());
    std::string command;

    if (ext == ".py") {
#ifdef _WIN32
        const std::string runner = is_command_available("py") ? "py" : "python";
        command = "start \"Python Output\" cmd /k " + runner + " " + quoted_path;
#elif defined(__APPLE__)
        command = "osascript -e 'tell application \"Terminal\" to do script \"python3 " +
                  path.string() + "\"'";
#else
        command = "x-terminal-emulator -e \"python3 " + path.string() + "; bash\"";
#endif
    } else if (ext == ".js") {
#ifdef _WIN32
        command = "start \"Node Output\" cmd /k node " + quoted_path;
#elif defined(__APPLE__)
        command = "osascript -e 'tell application \"Terminal\" to do script \"node " +
                  path.string() + "\"'";
#else
        command = "x-terminal-emulator -e \"node " + path.string() + "; bash\"";
#endif
    } else {
        return false;
    }

    return std::system(command.c_str()) == 0;
}

std::string trim_slug_separators(std::string value) {
    const size_t first = value.find_first_not_of("-_. ");
    if (first == std::string::npos) return "";

    const size_t last = value.find_last_not_of("-_. ");
    return value.substr(first, last - first + 1);
}

std::string sanitize_directory_name(const std::string& raw_name) {
    std::string result;
    bool previous_separator = false;

    for (unsigned char ch : raw_name) {
        if (std::isalnum(ch)) {
            result += static_cast<char>(std::tolower(ch));
            previous_separator = false;
        } else if (ch == '-' || ch == '_' || ch == ' ' || ch == '.' ||
                   ch == '/' || ch == '\\') {
            if (!result.empty() && !previous_separator) {
                result += '-';
                previous_separator = true;
            }
        }
    }

    result = trim_slug_separators(result);
    if (result.size() > 60) {
        result = trim_slug_separators(result.substr(0, 60));
    }

    return result;
}

bool is_path_text_char(unsigned char ch) {
    return std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.' ||
           ch == '/' || ch == '\\';
}

std::string clean_filename_part(std::string filename) {
    filename = trim_copy(filename);
    if (filename.size() >= 2) {
        const char first = filename.front();
        const char last = filename.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            filename = filename.substr(1, filename.size() - 2);
        }
    }
    return trim_copy(filename);
}

std::string first_file_stem_from_text(const std::string& text) {
    const std::string lower_text = lowercase_ascii(text);
    const std::vector<std::string> extensions = {
        ".html", ".htm", ".css", ".js", ".jsx", ".ts", ".tsx",
        ".cpp", ".c", ".h", ".hpp", ".py", ".java", ".json",
        ".md", ".txt"
    };

    size_t best_pos = std::string::npos;
    std::string best_extension;

    for (const auto& extension : extensions) {
        const size_t pos = lower_text.find(extension);
        if (pos != std::string::npos && pos < best_pos) {
            best_pos = pos;
            best_extension = extension;
        }
    }

    if (best_pos == std::string::npos) return "";

    size_t start = best_pos;
    while (start > 0 && is_path_text_char(static_cast<unsigned char>(text[start - 1]))) {
        start--;
    }

    const size_t end = best_pos + best_extension.size();
    const fs::path found_path(text.substr(start, end - start));
    return sanitize_directory_name(found_path.stem().string());
}

bool is_directory_stop_word(const std::string& word) {
    const std::vector<std::string> stop_words = {
        "a", "an", "the", "create", "make", "build", "generate",
        "file", "files", "code", "in", "it", "with", "using",
        "and", "or", "to", "for", "of", "this", "that", "please",
        "html", "css", "js", "javascript", "typescript", "put",
        "folder", "directory", "all", "particular", "based", "input",
        "name", "named", "called", "is", "ai", "agent", "like"
    };

    return std::find(stop_words.begin(), stop_words.end(), word) != stop_words.end();
}

std::string keyword_slug_from_text(const std::string& text) {
    std::vector<std::string> words;
    std::string current;

    for (unsigned char ch : text) {
        if (std::isalnum(ch)) {
            current += static_cast<char>(std::tolower(ch));
        } else if (!current.empty()) {
            if (!is_directory_stop_word(current) && current.size() > 1) {
                words.push_back(current);
                if (words.size() == 3) break;
            }
            current.clear();
        }
    }

    if (!current.empty() && words.size() < 3 &&
        !is_directory_stop_word(current) && current.size() > 1) {
        words.push_back(current);
    }

    std::string slug;
    for (const auto& word : words) {
        if (!slug.empty()) slug += "-";
        slug += word;
    }

    return sanitize_directory_name(slug);
}

std::string explicit_directory_name_from_request(const std::string& user_input) {
    const std::string lower_input = lowercase_ascii(user_input);
    const std::vector<std::string> markers = {
        "directory name is", "folder name is", "project name is",
        "directory named", "folder named", "project named",
        "directory called", "folder called", "project called"
    };

    for (const auto& marker : markers) {
        const size_t marker_pos = lower_input.find(marker);
        if (marker_pos == std::string::npos) continue;

        size_t pos = marker_pos + marker.size();
        while (pos < user_input.size() &&
               (user_input[pos] == ' ' || user_input[pos] == ':' ||
                user_input[pos] == '"' || user_input[pos] == '\'')) {
            pos++;
        }

        std::string phrase;
        int word_count = 0;
        std::string current;
        for (; pos < user_input.size(); pos++) {
            const unsigned char ch = static_cast<unsigned char>(user_input[pos]);
            if (std::isalnum(ch) || ch == '-' || ch == '_') {
                current += static_cast<char>(ch);
                continue;
            }

            if (!current.empty()) {
                phrase += phrase.empty() ? current : "-" + current;
                current.clear();
                word_count++;
                if (word_count == 3) break;
            }

            if (ch == '.' || ch == ',' || ch == ';' || ch == '\n' || ch == '\r') {
                break;
            }
        }

        if (!current.empty() && word_count < 3) {
            phrase += phrase.empty() ? current : "-" + current;
        }

        const std::string sanitized = sanitize_directory_name(phrase);
        if (!sanitized.empty()) return sanitized;
    }

    return "";
}

fs::path first_relative_path_component(const fs::path& path) {
    for (const auto& part : path) {
        const std::string value = part.string();
        if (value.empty() || value == ".") continue;
        return part;
    }
    return {};
}

bool has_real_parent_path(const fs::path& path) {
    if (path.is_absolute()) return true;

    const fs::path parent = path.parent_path();
    return !parent.empty() && parent.string() != ".";
}

struct CreateFileCommand {
    std::string filename;
    std::string content;
};

bool is_generic_file_stem(const std::string& stem) {
    const std::vector<std::string> generic_stems = {
        "index", "main", "style", "styles", "script", "app"
    };

    return std::find(generic_stems.begin(), generic_stems.end(), stem) != generic_stems.end();
}

std::string derive_directory_name_from_request(
    const std::string& user_input,
    const std::vector<CreateFileCommand>& commands) {

    std::string directory_name = explicit_directory_name_from_request(user_input);
    if (!directory_name.empty()) return directory_name;

    directory_name = first_file_stem_from_text(user_input);
    if (!directory_name.empty() && !is_generic_file_stem(directory_name)) {
        return directory_name;
    }

    directory_name = keyword_slug_from_text(user_input);
    if (!directory_name.empty()) return directory_name;

    for (const auto& command : commands) {
        const fs::path file_path(clean_filename_part(command.filename));
        const std::string stem = sanitize_directory_name(file_path.stem().string());
        if (!stem.empty() && !is_generic_file_stem(stem)) {
            return stem;
        }
    }

    return "generated-project";
}

fs::path common_relative_project_root(const std::vector<CreateFileCommand>& commands) {
    fs::path common_root;
    bool found_root = false;

    for (const auto& command : commands) {
        const fs::path requested_path(clean_filename_part(command.filename));
        if (requested_path.is_absolute() || !has_real_parent_path(requested_path)) {
            continue;
        }

        const fs::path first_component = first_relative_path_component(requested_path);
        if (first_component.empty()) continue;

        if (!found_root) {
            common_root = first_component;
            found_root = true;
        } else if (common_root != first_component) {
            return {};
        }
    }

    return found_root ? common_root : fs::path{};
}

bool user_requested_creation(const std::string& user_input) {
    const std::string lower_input = lowercase_ascii(user_input);
    const bool has_create_word =
        contains_word_ascii(lower_input, "create") ||
        contains_word_ascii(lower_input, "make") ||
        contains_word_ascii(lower_input, "build") ||
        contains_word_ascii(lower_input, "generate");

    if (!has_create_word) return false;

    const std::vector<std::string> creation_markers = {
        "file", "project", "folder", "directory",
        "website", "webpage", "page", "site", "portfolio",
        "html", "css", "javascript", "python", "cpp",
        ".html", ".css", ".js", ".cpp", ".py", ".txt", ".md", ".json"
    };

    for (const auto& marker : creation_markers) {
        const bool found_marker = marker.front() == '.'
            ? lower_input.find(marker) != std::string::npos
            : contains_word_ascii(lower_input, marker);

        if (found_marker) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> filenames_from_user_input(const std::string& user_input) {
    const std::string lower_input = lowercase_ascii(user_input);
    const std::vector<std::string> extensions = {
        ".html", ".htm", ".css", ".js", ".cpp", ".c", ".h", ".hpp",
        ".py", ".json", ".md", ".txt"
    };

    std::vector<std::string> filenames;
    for (const auto& extension : extensions) {
        size_t search_from = 0;
        while (true) {
            const size_t ext_pos = lower_input.find(extension, search_from);
            if (ext_pos == std::string::npos) break;

            size_t start = ext_pos;
            while (start > 0 &&
                   is_path_text_char(static_cast<unsigned char>(user_input[start - 1]))) {
                start--;
            }

            const size_t end = ext_pos + extension.size();
            std::string filename = clean_filename_part(user_input.substr(start, end - start));
            if (!filename.empty() &&
                std::find(filenames.begin(), filenames.end(), filename) == filenames.end()) {
                filenames.push_back(filename);
            }

            search_from = end;
        }
    }

    return filenames;
}

std::string portfolio_html_template() {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Portfolio</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <header class="hero">
        <nav>
            <h1>My Portfolio</h1>
            <div class="nav-links">
                <a href="#projects">Projects</a>
                <a href="#contact">Contact</a>
            </div>
        </nav>
        <section class="intro">
            <p class="eyebrow">Creative Developer</p>
            <h2>Building clean, useful, and thoughtful web experiences.</h2>
            <p>I design and develop responsive websites with a focus on simple structure, fast loading, and polished interfaces.</p>
            <a class="button" href="#projects">View Work</a>
        </section>
    </header>

    <main>
        <section id="projects" class="projects">
            <h2>Projects</h2>
            <div class="project-grid">
                <article class="project-card">
                    <h3>Portfolio Website</h3>
                    <p>A responsive personal portfolio made with HTML and CSS.</p>
                </article>
                <article class="project-card">
                    <h3>Landing Page</h3>
                    <p>A polished landing page layout for a modern product.</p>
                </article>
                <article class="project-card">
                    <h3>Dashboard UI</h3>
                    <p>A clean interface for tracking important information.</p>
                </article>
            </div>
        </section>

        <section id="contact" class="contact">
            <h2>Contact</h2>
            <p>Email: hello@example.com</p>
        </section>
    </main>
</body>
</html>
)";
}

std::string portfolio_css_template() {
    return R"(* {
    box-sizing: border-box;
}

body {
    margin: 0;
    font-family: Arial, sans-serif;
    color: #17202a;
    background: #f7f7fb;
}

a {
    color: inherit;
    text-decoration: none;
}

.hero {
    min-height: 72vh;
    padding: 28px 8%;
    background: linear-gradient(135deg, #101820, #284b63);
    color: #ffffff;
}

nav {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 20px;
}

.nav-links {
    display: flex;
    gap: 18px;
}

.intro {
    max-width: 760px;
    padding: 110px 0 70px;
}

.eyebrow {
    color: #f4d35e;
    font-weight: 700;
    text-transform: uppercase;
}

.intro h2 {
    margin: 0;
    font-size: 48px;
    line-height: 1.1;
}

.intro p {
    font-size: 18px;
    line-height: 1.7;
}

.button {
    display: inline-block;
    margin-top: 16px;
    padding: 12px 18px;
    border-radius: 6px;
    background: #f4d35e;
    color: #101820;
    font-weight: 700;
}

main {
    padding: 54px 8%;
}

.projects h2,
.contact h2 {
    font-size: 32px;
}

.project-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 18px;
}

.project-card {
    padding: 22px;
    border: 1px solid #e3e6ee;
    border-radius: 8px;
    background: #ffffff;
    box-shadow: 0 10px 28px rgba(16, 24, 32, 0.08);
}

.contact {
    margin-top: 54px;
    padding-top: 26px;
    border-top: 1px solid #dce1ea;
}

@media (max-width: 640px) {
    nav,
    .nav-links {
        align-items: flex-start;
        flex-direction: column;
    }

    .intro {
        padding-top: 72px;
    }

    .intro h2 {
        font-size: 34px;
    }
}
)";
}

std::string generic_file_template(const fs::path& filename, const std::string& user_input) {
    const std::string ext = lowercase_ascii(filename.extension().string());
    const std::string lower_input = lowercase_ascii(user_input);
    const std::string title = filename.stem().string().empty()
        ? "Generated File"
        : filename.stem().string();
    const bool wants_hello_world =
        lower_input.find("hello world") != std::string::npos ||
        lower_input.find("helloworld") != std::string::npos ||
        lowercase_ascii(filename.stem().string()).find("helloworld") != std::string::npos;

    if (ext == ".html" || ext == ".htm") {
        return "<!DOCTYPE html>\n"
               "<html lang=\"en\">\n"
               "<head>\n"
               "    <meta charset=\"UTF-8\">\n"
               "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
               "    <title>" + title + "</title>\n"
               "    <link rel=\"stylesheet\" href=\"style.css\">\n"
               "</head>\n"
               "<body>\n"
               "    <main class=\"page\">\n"
               "        <h1>" + title + "</h1>\n"
               "        <p>This page was generated from your request.</p>\n"
               "    </main>\n"
               "</body>\n"
               "</html>\n";
    }

    if (ext == ".css") {
        return "body {\n"
               "    margin: 0;\n"
               "    font-family: Arial, sans-serif;\n"
               "    color: #17202a;\n"
               "    background: #f7f7fb;\n"
               "}\n\n"
               ".page {\n"
               "    max-width: 960px;\n"
               "    margin: 0 auto;\n"
               "    padding: 64px 24px;\n"
               "}\n";
    }

    if (ext == ".js") {
        return wants_hello_world
            ? "console.log(\"Hello, World!\");\n"
            : "console.log(\"Generated from your request.\");\n";
    }
    if (ext == ".py") {
        return wants_hello_world
            ? "print(\"Hello, World!\")\n"
            : "print(\"Generated from your request.\")\n";
    }
    if (ext == ".cpp") {
        const std::string message = wants_hello_world ? "Hello, World!" : "Generated from your request.";
        return "#include <iostream>\n\n"
               "int main() {\n"
               "    std::cout << \"" + message + "\" << std::endl;\n"
               "    return 0;\n"
               "}\n";
    }
    if (ext == ".json") return "{\n    \"generated\": true\n}\n";
    if (ext == ".md") return "# " + title + "\n\nGenerated from your request.\n";

    return "Generated from your request: " + trim_copy(user_input) + "\n";
}

std::string build_fallback_create_response(const std::string& user_input) {
    if (!user_requested_creation(user_input)) return "";

    const std::string lower_input = lowercase_ascii(user_input);
    std::vector<std::string> filenames = filenames_from_user_input(user_input);

    if (filenames.empty()) {
    //    //if (lower_input.find("portfolio") != std::string::npos) {
    //    //    filenames.push_back("portfolio.html");
    //    //} else if (lower_input.find("html") != std::string::npos) {
    //    //    filenames.push_back("index.html");
    //    //} else if (lower_input.find("css") != std::string::npos) {
        //    filenames.push_back("style.css");
        //} else {
        //    filenames.push_back("index.html");
        //}
        // Disable hardcoded portfolio template → let LLM handle it
        //content = generic_file_template(file_path, user_input);
    }
    

    const bool asks_for_css = contains_word_ascii(lower_input, "css") ||
                              lower_input.find(".css") != std::string::npos ||
                              lower_input.find("and css") != std::string::npos ||
                              contains_word_ascii(lower_input, "style") ||
                              contains_word_ascii(lower_input, "styles");
    const bool has_html_file = std::any_of(filenames.begin(), filenames.end(), [](const std::string& filename) {
        const std::string ext = lowercase_ascii(fs::path(filename).extension().string());
        return ext == ".html" || ext == ".htm";
    });
    const bool has_css_file = std::any_of(filenames.begin(), filenames.end(), [](const std::string& filename) {
        return lowercase_ascii(fs::path(filename).extension().string()) == ".css";
    });

    if (has_html_file && asks_for_css && !has_css_file) {
        filenames.push_back("style.css");
    }

    std::string response;
    for (const auto& filename : filenames) {
        const fs::path file_path(filename);
        std::string content;
        //if (lower_input.find("portfolio") != std::string::npos) {
        //    const std::string ext = lowercase_ascii(file_path.extension().string());
        //    if (ext == ".html" || ext == ".htm") {
        //        content = portfolio_html_template();
        //    } else if (ext == ".css") {
        //        content = portfolio_css_template();
        //    } else {
        //        content = generic_file_template(file_path, user_input);
        //    }
        //} else {
        //    content = generic_file_template(file_path, user_input);
        //}
        // ALWAYS let generic template handle fallback (no hardcoded portfolio)
        content = generic_file_template(file_path, user_input);
        response += "create: " + filename + " content:\n" + content + "\n";
    }

    return response;
}

size_t find_next_tool_command(const std::string& response, size_t search_from) {
    const std::vector<std::string> command_keywords = {
        "create:", "read:", "list:", "analyze:",
        "summarize:", "delete:", "edit:", "run:"
    };

    for (size_t newline = response.find('\n', search_from);
         newline != std::string::npos;
         newline = response.find('\n', newline + 1)) {
        size_t line_start = newline + 1;

        while (line_start < response.size() &&
               (response[line_start] == ' ' || response[line_start] == '\t')) {
            line_start++;
        }

        for (const auto& keyword : command_keywords) {
            if (starts_with_at(response, line_start, keyword)) {
                return newline;
            }
        }
    }

    return std::string::npos;
}

bool contains_agent_command(const std::string& response) {
    const std::vector<std::string> command_keywords = {
        "create:", "read:", "list:", "analyze:",
        "summarize:", "delete:", "edit:", "run:"
    };

    for (const auto& keyword : command_keywords) {
        if (response.find(keyword) != std::string::npos) {
            return true;
        }
    }

    return false;
}

void strip_wrapping_code_fence(std::string& content) {
    content.erase(0, content.find_first_not_of(" \t\n\r"));

    if (starts_with_at(content, 0, "```")) {
        const size_t opening_end = content.find('\n');
        if (opening_end == std::string::npos) {
            content.clear();
            return;
        }

        content.erase(0, opening_end + 1);

        const size_t closing_start = content.find("\n```");
        if (closing_start != std::string::npos) {
            content = content.substr(0, closing_start);
        }
    }

    const size_t trailing_fence = content.rfind("\n```");
    if (trailing_fence != std::string::npos &&
        trim_copy(content.substr(trailing_fence)) == "```") {
        content = content.substr(0, trailing_fence);
    }
}

void trim_generated_tail(std::string& content, const fs::path& filename) {
    const std::string ext = lowercase_ascii(filename.extension().string());
    if (ext == ".html" || ext == ".htm") {
        const std::string lower_content = lowercase_ascii(content);
        const size_t html_end = lower_content.rfind("</html>");
        if (html_end != std::string::npos) {
            content = content.substr(0, html_end + 7);
            content += "\n";
        }
    }

    const std::vector<std::string> narrative_markers = {
        "\n===", "\nI've created", "\nI have created",
        "\nCreated file", "\nYour file", "\nThe files have been created"
    };
    const std::string lower_content = lowercase_ascii(content);
    size_t earliest_marker = std::string::npos;
    for (const auto& marker : narrative_markers) {
        const size_t marker_pos = lower_content.find(lowercase_ascii(marker));
        if (marker_pos != std::string::npos &&
            (earliest_marker == std::string::npos || marker_pos < earliest_marker)) {
            earliest_marker = marker_pos;
        }
    }

    if (earliest_marker != std::string::npos) {
        content = content.substr(0, earliest_marker);
    }

    const std::string meteo_tail = "\n\nAs LdragodestructorAI";
    const size_t tail_pos = content.find(meteo_tail);
    if (tail_pos != std::string::npos) {
        content = content.substr(0, tail_pos);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AGENTIC COMMAND PARSING & EXECUTION FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────────

/// Parse and execute "create: filename content: data" commands.
/// Multi-file responses are grouped in one project directory when the model
/// emits bare filenames such as "index.html" and "style.css".
bool execute_create_command(const std::string& response, const std::string& user_input) {
    std::vector<CreateFileCommand> commands;
    size_t search_from = 0;

    while (true) {
        const size_t create_pos = response.find("create:", search_from);
        if (create_pos == std::string::npos) break;

        const size_t content_pos = response.find("content:", create_pos);
        if (content_pos == std::string::npos) {
            std::cout << "\n[Agent Action Error] Missing 'content:' in create command" << std::endl;
            search_from = create_pos + 7;
            continue;
        }

        std::string filename_part = clean_filename_part(
            response.substr(create_pos + 7, content_pos - create_pos - 7));
        if (filename_part.empty()) {
            std::cout << "\n[Agent Action Error] Missing filename in create command" << std::endl;
            search_from = content_pos + 8;
            continue;
        }

        const size_t content_start = content_pos + 8;
        size_t content_end = find_next_tool_command(response, content_start);

        if (content_end == std::string::npos) {
            content_end = response.find("create:", content_start);
        }

        std::string content_part = content_end == std::string::npos
            ? response.substr(content_start)
            : response.substr(content_start, content_end - content_start);

        commands.push_back({filename_part, content_part});
        search_from = content_end == std::string::npos ? response.size() : content_end;
    }

    if (commands.empty()) return false;

    const std::string lower_input = lowercase_ascii(user_input);
    const bool requested_directory =
        lower_input.find("directory") != std::string::npos ||
        lower_input.find("folder") != std::string::npos ||
        lower_input.find("project") != std::string::npos;
    const bool should_group_files =
        commands.size() > 1 || requested_directory || user_requested_creation(user_input);

    fs::path group_dir;
    if (should_group_files) {
        group_dir = common_relative_project_root(commands);
        if (group_dir.empty()) {
            group_dir = derive_directory_name_from_request(user_input, commands);
        }

        if (!group_dir.empty()) {
            typewriter_print("\nCreating project folder: " +
                resolve_agent_path(group_dir.string()).string() + "\n", 6);
        }
    }

    bool created_any = false;
    std::vector<fs::path> created_paths;
    for (auto& command : commands) {
        fs::path requested_path(clean_filename_part(command.filename));
        fs::path output_path;

        if (requested_path.is_absolute()) {
            output_path = requested_path.lexically_normal();
        } else if (should_group_files && !group_dir.empty() &&
                   !has_real_parent_path(requested_path)) {
            output_path = resolve_agent_path((group_dir / requested_path).string());
        } else {
            output_path = resolve_agent_path(requested_path.string());
        }

        strip_wrapping_code_fence(command.content);
        trim_generated_tail(command.content, output_path);
        
// Auto-lint ALL file types
        std::string lint_msg = "lint: " + output_path.string() + " - check for errors and fix";
        std::cout << "[Auto-lint ALL] " << lint_msg << std::endl;



        try {
            typewriter_print("Typing into: " + output_path.string() + "\n", 6);

            if (output_path.has_parent_path()) {
                fs::create_directories(output_path.parent_path());
            }

            if (!write_file_with_visible_typing(output_path, command.content)) {
                std::cout << "\n[Agent Action Error] Cannot open file: " << output_path.string() << std::endl;
                continue;
            }

            if (!fs::exists(output_path)) {
                std::cout << "\n[Agent Action Error] File was not created: " << output_path.string() << std::endl;
                continue;
            }

            typewriter_print("Created: " + output_path.string() + "\n", 6);
            created_paths.push_back(output_path);
            created_any = true;
        } catch (const std::exception& e) {
            std::cout << "\n[Agent Action Error] " << e.what() << std::endl;
        }
    }

    const fs::path primary_file = choose_primary_created_file(created_paths);
    if (!primary_file.empty()) {
        typewriter_print("Opening: " + primary_file.string() + "\n", 6);
        if (!open_path_in_default_app(primary_file)) {
            typewriter_print("Ready: " + primary_file.string() + "\n", 6);
        }

        const std::string primary_ext = lowercase_ascii(primary_file.extension().string());
        if (primary_ext == ".py" || primary_ext == ".js") {
            typewriter_print("Running: " + primary_file.string() + "\n", 6);
            run_file_in_external_terminal(primary_file);
        }
    }

    // ── Show completion summary before clicking Go Live ──────────────────
    if (created_any) {
        std::cout << std::endl;
        std::cout << "=================================================" << std::endl;
        std::cout << "[TASK COMPLETED]" << std::endl;
        std::cout << "[Created Files]: " << created_paths.size() << std::endl;
        for (const auto& fpath : created_paths) {
            std::cout << "  >> " << fpath.filename().string() << std::endl;
        }
        std::cout << "=================================================" << std::endl;
        std::cout << std::endl;

        // ── Check if we created web files (HTML, CSS, JS) ──────────────────
        const fs::path primary_file = choose_primary_created_file(created_paths);
        bool has_web_files = false;
        
        for (const auto& fpath : created_paths) {
            const std::string ext = lowercase_ascii(fpath.extension().string());
            if (ext == ".html" || ext == ".htm" || ext == ".css" || ext == ".js") {
                has_web_files = true;
                break;
            }
        }

        // ── First, open the HTML file directly in browser ──────────────────
        if (!primary_file.empty() && has_web_files) {
            const std::string primary_ext = lowercase_ascii(primary_file.extension().string());
            
            // Open HTML file in browser
            if (primary_ext == ".html" || primary_ext == ".htm") {
                std::cout << "[Browser] Opening file in default browser..." << std::endl;
                
                // Get absolute path and ensure it exists
                fs::path abs_path = fs::absolute(primary_file);
                if (fs::exists(abs_path)) {
                    // Use proper Windows path quoting
                    std::string path_str = abs_path.string();
                    std::string browser_command = "start \"\" \"" + path_str + "\"";
                    
                    std::cout << "[Browser] File: " << path_str << std::endl;
                    int result = std::system(browser_command.c_str());
                    
                    if (result == 0) {
                        std::cout << "[Browser] SUCCESS: File opened in browser" << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                    } else {
                        std::cout << "[Browser] WARNING: Could not open file (trying alternative method)" << std::endl;
                        // Try alternative method
                        std::string alt_command = "explorer \"" + path_str + "\"";
                        std::system(alt_command.c_str());
                    }
                } else {
                    std::cout << "[Browser] ERROR: File does not exist at " << abs_path.string() << std::endl;
                }
            }
            
            // Then activate Go Live
            std::cout << "[Live Server] Activating Go Live for live reload..." << std::endl;
            click_go_live_button();
        }
    }

    return created_any;
}

/// Parse and execute "edit: filename" commands (requires user interaction for now)
bool execute_edit_command(const std::string& response) {
    size_t edit_pos = response.find("edit:");
    if (edit_pos == std::string::npos) return false;

    std::string filename_part = response.substr(edit_pos + 5);
    filename_part.erase(0, filename_part.find_first_not_of(" \t\n\r"));
    
    // Stop at newline or next command
    size_t end_pos = filename_part.find_first_of("\n");
    if (end_pos != std::string::npos) {
        filename_part = filename_part.substr(0, end_pos);
    }
    filename_part.erase(filename_part.find_last_not_of(" \t\n\r") + 1);

    // For edit, we'd need interactive feedback. For now, just log the intent.
    std::cout << "\n[Agent Action] Edit requested for: " << filename_part << std::endl;
    std::cout << "[System] Edit mode requires additional context. Provide updated content." << std::endl;
    return true;
}

/// Parse and execute "run: command" with safety whitelist
bool execute_run_command(const std::string& response) {
    size_t run_pos = response.find("run:");
    if (run_pos == std::string::npos) return false;

    std::string cmd = response.substr(run_pos + 4);
    cmd.erase(0, cmd.find_first_not_of(" \t\n\r"));
    
    // Stop at newline or next command
    size_t end_pos = cmd.find_first_of("\n");
    if (end_pos != std::string::npos) {
        cmd = cmd.substr(0, end_pos);
    }

    // SAFETY WHITELIST: Only allow safe, non-destructive commands
    std::vector<std::string> whitelist_prefixes = {
        "echo", "dir", "ls", "cat", "type", "mkdir", "copy", "cp",
        "python", "node", "npm", "cmake", "make", "gcc", "g++", "cl"
    };

    bool is_safe = false;
    for (const auto& prefix : whitelist_prefixes) {
        if (cmd.find(prefix) == 0) {
            is_safe = true;
            break;
        }
    }

    if (!is_safe) {
        std::cout << "\n[Agent Action] ⚠ Command BLOCKED by whitelist: " << cmd << std::endl;
        return false;
    }

    std::cout << "\n[Agent Action] Running: " << cmd << std::endl;
    int result = system(cmd.c_str());
    std::cout << "[System] Exit code: " << result << std::endl;
    return result == 0;
}

/// Parse and execute "read: filename" commands (read file contents)
bool execute_read_command(const std::string& response) {
    // Remove markdown code blocks for parsing
    std::string clean_response = response;
    size_t start = 0;
    while ((start = clean_response.find("```", start)) != std::string::npos) {
        clean_response.erase(start, 3);
    }
    
    size_t read_pos = clean_response.find("read:");
    if (read_pos == std::string::npos) return false;

    std::string filename = response.substr(read_pos + 5);
    filename.erase(0, filename.find_first_not_of(" \t\n\r"));
    
    size_t end_pos = filename.find_first_of("\n");
    if (end_pos != std::string::npos) {
        filename = filename.substr(0, end_pos);
    }
    filename.erase(filename.find_last_not_of(" \t\n\r") + 1);

    try {
        if (!fs::exists(filename)) {
            std::cout << "\n[Agent Action] ✗ File not found: " << filename << std::endl;
            return false;
        }

        std::ifstream infile(filename);
        if (!infile.is_open()) {
            std::cout << "\n[Agent Action] ✗ Cannot open file: " << filename << std::endl;
            return false;
        }

        std::cout << "\n[Agent Action] Reading file: " << filename << std::endl;
        std::cout << "────────────────────────────────────────────────" << std::endl;
        
        std::string line;
        int line_num = 1;
        while (std::getline(infile, line)) {
            std::cout << line_num << ": " << line << std::endl;
            line_num++;
        }
        
        std::cout << "────────────────────────────────────────────────" << std::endl;
        infile.close();
        return true;
    } catch (const std::exception& e) {
        std::cout << "\n[Agent Action] ✗ Error reading file: " << e.what() << std::endl;
        return false;
    }
}

/// Parse and execute "list: directory" commands (list directory contents)
bool execute_list_command(const std::string& response) {
    // Remove markdown code blocks for parsing
    std::string clean_response = response;
    size_t start = 0;
    while ((start = clean_response.find("```", start)) != std::string::npos) {
        clean_response.erase(start, 3);
    }
    
    size_t list_pos = clean_response.find("list:");
    if (list_pos == std::string::npos) return false;

    std::string dirname = response.substr(list_pos + 5);
    dirname.erase(0, dirname.find_first_not_of(" \t\n\r"));
    
    size_t end_pos = dirname.find_first_of("\n");
    if (end_pos != std::string::npos) {
        dirname = dirname.substr(0, end_pos);
    }
    dirname.erase(dirname.find_last_not_of(" \t\n\r") + 1);

    if (dirname.empty()) dirname = ".";

    try {
        if (!fs::exists(dirname)) {
            std::cout << "\n[Agent Action] ✗ Directory not found: " << dirname << std::endl;
            return false;
        }

        std::cout << "\n[Agent Action] Listing directory: " << dirname << std::endl;
        std::cout << "────────────────────────────────────────────────" << std::endl;
        
        int count = 0;
        for (const auto& entry : fs::directory_iterator(dirname)) {
            std::string type = entry.is_directory() ? "[DIR]" : "[FILE]";
            std::cout << type << " " << entry.path().filename().string();
            
            if (entry.is_regular_file()) {
                std::cout << " (" << entry.file_size() << " bytes)";
            }
            std::cout << std::endl;
            count++;
        }
        
        std::cout << "────────────────────────────────────────────────" << std::endl;
        std::cout << "[System] Total items: " << count << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "\n[Agent Action] ✗ Error listing directory: " << e.what() << std::endl;
        return false;
    }
}

/// Parse and execute "analyze: filename" commands (analyze file structure)
bool execute_analyze_command(const std::string& response) {
    // Remove markdown code blocks for parsing
    std::string clean_response = response;
    size_t start = 0;
    while ((start = clean_response.find("```", start)) != std::string::npos) {
        clean_response.erase(start, 3);
    }
    
    size_t analyze_pos = clean_response.find("analyze:");
    if (analyze_pos == std::string::npos) return false;

    std::string filename = response.substr(analyze_pos + 8);
    filename.erase(0, filename.find_first_not_of(" \t\n\r"));
    
    size_t end_pos = filename.find_first_of("\n");
    if (end_pos != std::string::npos) {
        filename = filename.substr(0, end_pos);
    }
    filename.erase(filename.find_last_not_of(" \t\n\r") + 1);

    try {
        if (!fs::exists(filename)) {
            std::cout << "\n[Agent Action] ✗ File not found: " << filename << std::endl;
            return false;
        }

        std::ifstream infile(filename);
        if (!infile.is_open()) {
            std::cout << "\n[Agent Action] ✗ Cannot open file: " << filename << std::endl;
            return false;
        }

        // Analyze file structure
        int line_count = 0;
        int word_count = 0;
        int char_count = 0;
        std::string line;

        while (std::getline(infile, line)) {
            line_count++;
            char_count += line.length() + 1; // +1 for newline
            
            std::istringstream words(line);
            std::string word;
            while (words >> word) {
                word_count++;
            }
        }

        infile.close();

        std::cout << "\n[Agent Action] File Analysis: " << filename << std::endl;
        std::cout << "────────────────────────────────────────────────" << std::endl;
        std::cout << "Lines:      " << line_count << std::endl;
        std::cout << "Words:      " << word_count << std::endl;
        std::cout << "Characters: " << char_count << std::endl;
        
        // Get file size
        auto file_size = fs::file_size(filename);
        std::cout << "Size:       " << file_size << " bytes" << std::endl;
        std::cout << "────────────────────────────────────────────────" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "\n[Agent Action] ✗ Error analyzing file: " << e.what() << std::endl;
        return false;
    }
}

/// Parse and execute "summarize: filename" commands (read and output file for summary)
bool execute_summarize_command(const std::string& response) {
    // Remove markdown code blocks for parsing
    std::string clean_response = response;
    size_t start = 0;
    while ((start = clean_response.find("```", start)) != std::string::npos) {
        clean_response.erase(start, 3);
    }
    
    size_t summarize_pos = clean_response.find("summarize:");
    if (summarize_pos == std::string::npos) return false;

    std::string filename = response.substr(summarize_pos + 10);
    filename.erase(0, filename.find_first_not_of(" \t\n\r"));
    
    size_t end_pos = filename.find_first_of("\n");
    if (end_pos != std::string::npos) {
        filename = filename.substr(0, end_pos);
    }
    filename.erase(filename.find_last_not_of(" \t\n\r") + 1);

    try {
        if (!fs::exists(filename)) {
            std::cout << "\n[Agent Action] ✗ File not found: " << filename << std::endl;
            return false;
        }

        std::ifstream infile(filename);
        if (!infile.is_open()) {
            std::cout << "\n[Agent Action] ✗ Cannot open file: " << filename << std::endl;
            return false;
        }

        std::cout << "\n[Agent Action] Retrieving for summarization: " << filename << std::endl;
        std::cout << "────────────────────────────────────────────────" << std::endl;
        
        std::string full_content;
        std::string line;
        while (std::getline(infile, line)) {
            full_content += line + "\n";
        }
        infile.close();
        
        // Return content to agent in next response (agent can then summarize)
        std::cout << full_content << std::endl;
        std::cout << "────────────────────────────────────────────────" << std::endl;
        std::cout << "[System] File content provided. You may now summarize this in your response." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "\n[Agent Action] ✗ Error summarizing file: " << e.what() << std::endl;
        return false;
    }
}

/// Parse and execute "delete: filename" commands (delete files with confirmation)
bool execute_delete_command(const std::string& response) {
    // Remove markdown code blocks for parsing
    std::string clean_response = response;
    size_t start = 0;
    while ((start = clean_response.find("```", start)) != std::string::npos) {
        clean_response.erase(start, 3);
    }
    
    size_t delete_pos = clean_response.find("delete:");
    if (delete_pos == std::string::npos) return false;

    std::string filename = response.substr(delete_pos + 7);
    filename.erase(0, filename.find_first_not_of(" \t\n\r"));
    
    size_t end_pos = filename.find_first_of("\n");
    if (end_pos != std::string::npos) {
        filename = filename.substr(0, end_pos);
    }
    filename.erase(filename.find_last_not_of(" \t\n\r") + 1);

    try {
        if (!fs::exists(filename)) {
            std::cout << "\n[Agent Action] ✗ File not found: " << filename << std::endl;
            return false;
        }

        std::cout << "\n[Agent Action] ⚠ Delete file: " << filename << std::endl;
        std::cout << "[System] WARNING: Cannot delete - requires user confirmation." << std::endl;
        std::cout << "[System] If you approve, manually execute: " << filename << std::endl;
        return false;  // Safety: require user confirmation
    } catch (const std::exception& e) {
        std::cout << "\n[Agent Action] ✗ Error deleting file: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    try {
        const fs::path workspace_root = detect_workspace_root();
        fs::current_path(workspace_root);
        std::cout << "[System] Workspace root: " << workspace_root.string() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Warning] Could not set workspace root: " << e.what() << std::endl;
    }

    std::cout << "[Step 1] Initializing TurboQuant..." << std::endl;
    int target_dim = 1024;
    TurboQuant tq(target_dim, 2);

    llama_model_params m_params = llama_model_default_params();
    llama_log_set([](enum ggml_log_level level, const char* text, void*) {
        // Only print errors, suppress info/warnings
        if (level == GGML_LOG_LEVEL_ERROR) {
            std::cerr << text;
        }
    }, nullptr);
    std::cout << "[Step 2] Loading Model File..." << std::endl;
    llama_model* model = llama_load_model_from_file("models/phi3.gguf", m_params);

    if (!model) {
        std::cerr << "[Error] Model nahi mila! Check models/phi3.gguf" << std::endl;
        return 1;
    }

    llama_context_params c_params = llama_context_default_params();
    c_params.n_ctx = 2048; // FIX 1: was 128 — too small for any real output
    llama_context* ctx = llama_new_context_with_model(model, c_params);
    const struct llama_vocab* vocab = llama_model_get_vocab(model);

    if (!ctx) {
        std::cerr << "[Error] Context build fail ho gaya!" << std::endl;
        llama_free_model(model);
        return 1;
    }

    // FIX 2: Pre-allocate a reusable single-token batch with proper storage.
    // llama_batch_get_one() returns a batch pointing to internal/stack memory —
    // writing .pos/.seq_id/.logits back into it is undefined behaviour and
    // breaks position tracking, causing the model to stop after one token.
    llama_token  gen_token_buf[1];
    int32_t      gen_pos[1];
    int32_t      gen_nseq[1];
    int32_t*     gen_seqid_ptrs[1];
    int32_t      gen_seqid0[1] = {0};
    int8_t       gen_logits[1];

    llama_batch gen_batch;
    gen_batch.n_tokens  = 1;
    gen_batch.token     = gen_token_buf;
    gen_batch.embd      = nullptr;
    gen_batch.pos       = gen_pos;
    gen_batch.n_seq_id  = gen_nseq;
    gen_batch.seq_id    = gen_seqid_ptrs;
    gen_batch.logits    = gen_logits;
    gen_batch.seq_id[0] = gen_seqid0;

    // ── Sampling hyperparameters ─────────────────────────────────────────────
    // Tune these to control output diversity vs. focus.
    const int   TOP_K       = 40;    // keep only the 40 highest-logit tokens
    const float TOP_P       = 0.90f; // nucleus: trim until cumulative prob >= 90%
    const float TEMPERATURE = 0.80f; // < 1.0 = sharper, > 1.0 = flatter distribution

    // Seeded once at startup; used in the generation loop below.
    static std::mt19937 rng(std::random_device{}());

    // ── Updated System Prompt with Agentic Capabilities ────────────────────
    // This tells the model what actions it can perform
    std::string system_prompt =
        "You are LdragodestructorAI, an Agentic AI assistant with comprehensive file-system access and web development capabilities. "
        "You have the following capabilities:\n"
        "1. FILE CREATION: Use 'create: filename content: file_content' to create files. "
        "For multi-line files, put the full file content after 'content:' and do not add narration after the file content. "
        "When a request needs multiple files, put every file in one lowercase project directory based on the user's request, "
        "for example 'create: portfolio/portfolio.html content: ...' and 'create: portfolio/style.css content: ...'.\n"
        "2. FILE READING: Use 'read: filename' to read and display file contents\n"
        "3. FILE ANALYSIS: Use 'analyze: filename' to get file statistics (lines, words, size)\n"
        "4. FILE LISTING: Use 'list: directory' to list directory contents\n"
        "5. FILE SUMMARIZATION: Use 'summarize: filename' to retrieve content for summarization\n"
        "6. FILE DELETION: Use 'delete: filename' to delete files (requires user confirmation)\n"
        "7. FILE EDITING: Use 'edit: filename' to edit files\n"
        "8. COMMAND EXECUTION: Use 'run: command' to execute system commands (limited to safe commands)\n"
        "9. WEB DEVELOPMENT AUTO-LAUNCH: When you create HTML, CSS, or JS files, the system will automatically launch VS Code Live Server for instant browser preview.\n"
        "10. PERSISTENT MEMORY: You remember all previous conversation turns in this session.\n"
        "Always identify yourself as LdragodestructorAI when asked about your name. "
        "Be helpful, professional, and self-correct when TurboQuant gain is low.";

    std::string user_input;
    std::cout << "\n--- LdragodestructorAI Agentic System Ready (Type 'exit' to quit) ---\n";

    // ── Persistent memory tracking across turns ────────────────────────────
    int n_past = 0;  // Total tokens in KV cache from all previous turns
    bool first_turn = true;  // Track if this is the first turn

    while (true) {
        std::cout << "\nYou: ";
        if (!std::getline(std::cin, user_input) || user_input == "exit") break;

        user_input.erase(std::remove(user_input.begin(), user_input.end(), '\r'),
                         user_input.end());
        if (user_input.empty()) continue;

        //const std::string immediate_create_response = build_fallback_create_response(user_input);
        //if (!immediate_create_response.empty()) {
        //    std::cout << "Agent: ";
        //    typewriter_print("Creating from your request...\n", 6);
        //    execute_create_command(immediate_create_response, user_input);
        //    continue;
        //}

        // ── MEMORY: Format this turn WITHOUT resetting context ──────────────
        // For the first turn, prepend system prompt
        std::string turn_input;
        if (first_turn) {
            turn_input = "<|system|>\n" + system_prompt + "<|end|>\n" +
                        "<|user|>\n" + user_input + "<|end|>\n<|assistant|>\n";
            first_turn = false;
        } else {
            // Subsequent turns: just append user message
            turn_input = "<|user|>\n" + user_input + "<|end|>\n<|assistant|>\n";
        }

        std::cout << "[Step 3] Tokenizing turn..." << std::endl;
        std::vector<llama_token> turn_tokens(turn_input.size() + 32);
        const bool add_bos = n_past == 0;
        int n_turn = llama_tokenize(
            vocab,
            turn_input.c_str(),
            (int)turn_input.size(),
            turn_tokens.data(),
            (int)turn_tokens.size(),
            add_bos,
            true);  // special tokens
        if (n_turn <= 0) continue;
        turn_tokens.resize(n_turn);

        // ── KV Cache Management: Check for overflow ────────────────────────
        if (n_past + n_turn > c_params.n_ctx) {
            std::cout << "[System] Context full (" << n_past << " + " << n_turn 
                    << " > " << c_params.n_ctx << "). Clearing old memory..." << std::endl;
            // Free and recreate context to clear KV cache
            llama_free(ctx);
            ctx = llama_new_context_with_model(model, c_params);
            if (!ctx) {
                std::cerr << "[Error] Context recreation failed!" << std::endl;
                continue;
            }
            n_past = 0;
        }

        std::cout << "[Step 4] Decoding " << n_turn << " tokens (KV pos: " << n_past << ")..." << std::endl;

        // ── Build turn batch with correct position tracking ────────────────
        std::vector<int32_t>  turn_pos(n_turn);
        std::vector<int32_t>  turn_nseq(n_turn, 1);
        std::vector<int32_t*> turn_seqid_ptrs(n_turn);
        std::vector<int32_t>  turn_seqid0(n_turn, 0);
        std::vector<int8_t>   turn_logits(n_turn, 0);

        for (int i = 0; i < n_turn; i++) {
            turn_pos[i]        = n_past + i;  // position in full KV cache
            turn_seqid_ptrs[i] = &turn_seqid0[i];
        }
        turn_logits[n_turn - 1] = 1; // only need logits for the last token

        llama_batch turn_batch;
        turn_batch.n_tokens  = n_turn;
        turn_batch.token     = turn_tokens.data();
        turn_batch.embd      = nullptr;
        turn_batch.pos       = turn_pos.data();
        turn_batch.n_seq_id  = turn_nseq.data();
        turn_batch.seq_id    = turn_seqid_ptrs.data();
        turn_batch.logits    = turn_logits.data();

        std::cout << "[Step 5] Calling llama_decode..." << std::endl;
        if (llama_decode(ctx, turn_batch) != 0) {
            std::cerr << "[Error] Decode failed!" << std::endl;
            continue;
        }

        n_past += n_turn;  // Update position tracker

        // ── Generation loop with Response Capture ──────────────────────────
        std::cout << "Agent: ";
        int n_cur = n_past;

        float last_mse2 = 0.0f;
        float last_mse3 = 0.0f;
        float last_gain = 0.0f;

        std::string full_response = "";  // Capture all generated text

        for (int i = 0; i < 1024; i++) {
            float* logits = llama_get_logits(ctx);
            if (!logits) break;

            // ── TurboQuant logit processing ────────────────────────────────
            std::vector<float> test_logits(logits, logits + target_dim);

            // Stage 1 & 2: Compression
            std::vector<int> compressed_indices = tq.compress_mse(test_logits);

            // Reconstruct Stage 2 (Base Quantization)
            std::vector<float> x_tilde(target_dim);
            float mse_stage2 = 0;
            for (int j = 0; j < target_dim; ++j) {
                x_tilde[j]  = tq.centroids[compressed_indices[j]];
                mse_stage2 += std::pow(test_logits[j] - x_tilde[j], 2);
            }
            mse_stage2 /= target_dim;

            // Stage 3: Apply QJL Residuals
            tq.apply_qjl_residual(test_logits, compressed_indices);

            // Dynamic delta calculation
            float sum_abs_residual = 0;
            for (int j = 0; j < target_dim; ++j) {
                float res = std::abs(test_logits[j] - tq.centroids[compressed_indices[j]]);
                sum_abs_residual += res;
            }
            float dynamic_delta = sum_abs_residual / target_dim;

            // Reconstruct Stage 3 (Using Dynamic Delta)
            std::vector<float> corrected_logits =
                tq.decompress_with_qjl(compressed_indices, dynamic_delta);

            float mse_stage3 = 0;
            for (int j = 0; j < target_dim; ++j) {
                mse_stage3 += std::pow(test_logits[j] - corrected_logits[j], 2);
            }
            mse_stage3 /= target_dim;

            float improvement = (1.0f - (mse_stage3 / mse_stage2)) * 100.0f;

            last_mse2 = mse_stage2;
            last_mse3 = mse_stage3;
            last_gain = improvement;

            // ── Self-Correction Logic: If gain is very low, model might be confused ─
            if (improvement < 5.0f && i > 100) {
                std::cout << "\n[TurboQuant Alert] Low gain (" << improvement 
                        << "%) detected. Agent may need clarification.\n";
            }

            // ── Sampling: Top-K → Top-P (nucleus) → Temperature ───────────
            int vocab_size = llama_n_vocab(vocab);

            // Stage 1 – Top-K
            int k = std::min(TOP_K, vocab_size);
            std::vector<std::pair<float, int>> scored(vocab_size);
            for (int id = 0; id < vocab_size; ++id)
                scored[id] = {logits[id], id};

            std::nth_element(
                scored.begin(),
                scored.begin() + k,
                scored.end(),
                [](const auto& a, const auto& b){ return a.first > b.first; });
            scored.resize(k);

            // Sort the surviving K by descending logit
            std::sort(scored.begin(), scored.end(),
                      [](const auto& a, const auto& b){ return a.first > b.first; });

            // Stage 2 – Temperature scaling + softmax
            float max_l = scored[0].first;
            std::vector<float> probs(k);
            float sum = 0.0f;
            for (int j = 0; j < k; ++j) {
                probs[j]  = std::exp((scored[j].first - max_l) / TEMPERATURE);
                sum      += probs[j];
            }
            for (float& p : probs) p /= sum;

            // Stage 3 – Top-P (nucleus) trimming
            float cumulative  = 0.0f;
            int nucleus_size  = k;
            for (int j = 0; j < k; ++j) {
                cumulative += probs[j];
                if (cumulative >= TOP_P) {
                    nucleus_size = j + 1;
                    break;
                }
            }

            // Re-normalise the nucleus slice
            float nucleus_sum = 0.0f;
            for (int j = 0; j < nucleus_size; ++j) nucleus_sum += probs[j];
            for (int j = 0; j < nucleus_size; ++j) probs[j] /= nucleus_sum;

            // Weighted random sample from the nucleus
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            float r     = dist(rng);
            int chosen  = nucleus_size - 1;
            cumulative  = 0.0f;
            for (int j = 0; j < nucleus_size; ++j) {
                cumulative += probs[j];
                if (r <= cumulative) { chosen = j; break; }
            }

            llama_token next_token = scored[chosen].second;

            if (llama_token_is_eog(vocab, next_token)) {
                if (!full_response.empty()) break;

                auto replacement = std::find_if(
                    scored.begin(),
                    scored.end(),
                    [vocab](const auto& candidate) {
                        return !llama_token_is_eog(vocab, candidate.second);
                    });

                if (replacement == scored.end()) break;
                next_token = replacement->second;
            }

            // Decode token text and capture it for the UI/action layer.
            char buf[128];
            int n = llama_token_to_piece(vocab, next_token, buf, sizeof(buf), 0, false);
            if (n > 0) {
                std::string piece(buf, n);
                full_response += piece;
            }

            // Slot the new token into the safe batch at correct KV position
            gen_batch.token[0]    = next_token;
            gen_batch.pos[0]      = n_cur;
            gen_batch.n_seq_id[0] = 1;
            gen_batch.seq_id[0]   = gen_seqid0;
            gen_batch.logits[0]   = 1;

            n_cur++;

            if (llama_decode(ctx, gen_batch) != 0) {
                std::cout << " [Decode Error]";
                break;
            }
        }
        const bool has_agent_command = contains_agent_command(full_response);
        const bool has_create_command = full_response.find("create:") != std::string::npos;
        const std::string fallback_create_response = has_create_command
            ? ""
            : build_fallback_create_response(user_input);
        const bool has_fallback_create = !fallback_create_response.empty();

        if (!has_agent_command && !has_fallback_create) {
            if (!trim_copy(full_response).empty()) {
                typewriter_print(full_response, 8);
            } else {
                typewriter_print("I could not generate a response for that turn.", 8);
            }
            std::cout << std::endl;
            std::cout << "[TurboQuant] Final MSE Stage 2: " << last_mse2
                    << " | Stage 3: " << last_mse3
                    << " | Gain: "    << last_gain << "%" << std::endl;
        } else {
            std::cout << std::endl;
        }

        // ── Update persistent memory position for next turn ──────────────── 
        n_past = n_cur;  // n_cur tracks all tokens (input + generated)

        // ── AGENT COMMAND PARSING & EXECUTION ──────────────────────────────
        // Intercept and execute commands from the model's response
        if (has_create_command) {
            execute_create_command(full_response, user_input);
        } else if (has_fallback_create) {
            typewriter_print("Creating from your request...\n", 6);
            execute_create_command(fallback_create_response, user_input);
        }
        if (!has_fallback_create && full_response.find("read:") != std::string::npos) {
            execute_read_command(full_response);
        }
        if (!has_fallback_create && full_response.find("list:") != std::string::npos) {
            execute_list_command(full_response);
        }
        if (!has_fallback_create && full_response.find("analyze:") != std::string::npos) {
            execute_analyze_command(full_response);
        }
        if (!has_fallback_create && full_response.find("summarize:") != std::string::npos) {
            execute_summarize_command(full_response);
        }
        if (!has_fallback_create && full_response.find("delete:") != std::string::npos) {
            execute_delete_command(full_response);
        }
        if (!has_fallback_create && full_response.find("edit:") != std::string::npos) {
            execute_edit_command(full_response);
        }
        if (!has_fallback_create && full_response.find("run:") != std::string::npos) {
            execute_run_command(full_response);
        }
        
    }

    llama_free(ctx);
    llama_free_model(model);
    return 0;
}
