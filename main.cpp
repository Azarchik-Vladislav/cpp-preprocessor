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

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void PrintError(const std::string& name_file, const path& current_directory, const int& line) {
    std::cout << "unknown include file "s << name_file << " at file "s << current_directory.string()
              << " at line "s << line << std::endl;
}

pair<path, bool> IsIncludePath(const path& current_path, const vector<path>& include_directories){
    path full_path;

    for(const auto& dir : include_directories) {
        full_path = dir / current_path;
        full_path.make_preferred();
            
        if(filesystem::exists(full_path)){
           return {full_path,true};
           break;
        }
    }
    
    return {""_p, false};
}

bool Preprocess (istream& in, ostream& out, const  path& in_file, const vector<path>& include_directories) {
    static regex include_from_current_dir(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex include_from_vec_of_dir(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

    smatch matched;
    int line_in_file = 0;
    std::string str;
    
    while(getline(in, str)){
        ++line_in_file;

        if(regex_match(str, matched, include_from_current_dir)) {
            path current_path = string(matched[1]);
            path full_path = in_file.parent_path() / current_path;
            full_path.make_preferred();

            ifstream in_current_directory(full_path);
            if(filesystem::exists(full_path)){
                Preprocess(in_current_directory, out, full_path, include_directories);
            } else if (!filesystem::exists(full_path)) {
                const auto& [path_name, value] = IsIncludePath(current_path, include_directories);

                if(value){
                    ifstream in_current_directory(path_name);
                    Preprocess(in_current_directory, out, path_name, include_directories);
                } else {
                    PrintError(string(matched[1]), in_file, line_in_file);
                    return false;
                } 
            } else {
                PrintError(string(matched[1]), in_file, line_in_file);
                return false;
            } 
        } else if(regex_match(str, matched, include_from_vec_of_dir)) {
            path current_path = string(matched[1]);
            const auto& [path_name, value] = IsIncludePath(current_path, include_directories);

            if(value){
                ifstream in_current_directory(path_name);
                Preprocess(in_current_directory, out, path_name, include_directories);
            } else {
                PrintError(string(matched[1]), in_file, line_in_file);
                return false;
            }
        } else {
            out << str << std::endl;
        }
    }

    return true;
}

// напишите эту функцию
bool IsOpenFiles (const path& in_file, const path& out_file, const vector<path>& include_directories) {
    if (!filesystem::exists(in_file)) {
        return false;
    }

    ifstream src(in_file, ios::in|ios::binary);
    if (!src) {
        return false;
    }

    ofstream dst(out_file, ios::out|ios::binary);

    if(!dst){
        return false;
    }

    return Preprocess(src, dst, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);
    
    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
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

    assert((!IsOpenFiles("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

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
