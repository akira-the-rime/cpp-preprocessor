#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

bool MESSAGE = true;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void MakePath(const string& match, path& to_source) {
    string temp;
    for (size_t i = 0; i < match.size(); ++i) {
        if (match[i] == '/') {
            to_source = to_source / temp;
            temp.clear();
            continue;
        }
        temp += match[i];
    }
    to_source = to_source / temp;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories);

bool SearchIncludeDirectories(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    for (auto it = include_directories.begin(); it != include_directories.end(); ++it) {
        vector<path> new_directories;
        if (filesystem::exists(*it)) {
            for (const auto& gotten_path : filesystem::directory_iterator(*it)) {
                if (gotten_path.status().type() == filesystem::file_type::directory) {
                    new_directories.push_back(gotten_path.path());
                }
                else if (gotten_path.path().filename() == in_file.filename()) {
                    Preprocess(gotten_path.path(), out_file, include_directories);
                    return true;
                }
            }
            bool found = SearchIncludeDirectories(in_file, out_file, new_directories);
            if (found) {
                return true;
            }
            new_directories.clear();
        }
    }
    return false;
}

bool Process(const path& out_file, const vector<path>& include_directories, const string& match, const path& current_path) {
    path to_source = current_path;
    MakePath(match, to_source);
    bool success = Preprocess(to_source, out_file, include_directories);
    if (!success) {
        success = SearchIncludeDirectories(to_source, out_file, include_directories);
        return success;
    }
    return true;
}

// напишите эту функцию
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {

    ifstream fin(in_file, ios::in);

    if (!fin) {
        return false;
    }

    int lines = 1;
    ofstream fout(out_file, ios::out | ios::app);
    string include;
    const path current_path = in_file.parent_path();
    const regex match_quotes(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    const regex match_angle_brackets(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

    while (!fin.eof()) {
        getline(fin, include);
        if (fin.eof()) {
            if (!include.empty()) {
                fout << include << endl;
            }
            return true;
        }
        bool success = true;
        std::smatch m;
        if (regex_match(include, m, match_quotes) || regex_match(include, m, match_angle_brackets)) {
            success = Process(out_file, include_directories, m[1].str(), current_path);
        }
        else {
            fout << include << endl;
            include.clear();
        }
        if (!success && MESSAGE) {
            cout << "unknown include file " << m[1].str() << " at file " << in_file.string() << " at line " << lines << endl;
            MESSAGE = !MESSAGE;
            return false;
        }
        ++lines;
    }
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() {
    setlocale(LC_ALL, "ru");
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    {
        assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
            { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));
    }

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}