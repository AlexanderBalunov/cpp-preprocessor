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

vector<path>::const_iterator SearchInIncludeDirectories(
                             const vector<path>& directories, const path& p) {
    return find_if(directories.begin(), directories.end(), 
                  [&p](const path& incl_p) {
                      return filesystem::exists(incl_p / p);
                  });    
}

bool Preprocess(const path& in_file, const path& out_file, 
                const vector<path>& include_directories) {
    ifstream input_file(in_file);
    if (!input_file.is_open()) {
        cerr << "Can't open input file"s << endl;
        return false;
    }
    
    ofstream output_file(out_file, ios::out | ios::app);
    if (!output_file.is_open()) {
        cerr << "Can't open output file"s << endl;
        return false;
    }
    
    const static regex incl_custom_reg(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    const static regex incl_std_reg(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch file_name;
    
    string line;
    int line_number = 0;
    while (++line_number, getline(input_file, line)) {
        path full_path;
        if (regex_match(line, file_name, incl_custom_reg)) {
            path p(file_name[1]);
            full_path = in_file.parent_path() / p; 
            if (!filesystem::exists(in_file.parent_path() / p)) {
                const auto search_result = 
                SearchInIncludeDirectories(include_directories, p);
                if (search_result == include_directories.end()) {
                    cout << "unknown include file "s << p.filename().string() 
                         << " at file "s << in_file.string() 
                         << " at line "s << line_number << endl;
                    return false;
                }
                full_path = *search_result / p;
                Preprocess(full_path, out_file, include_directories);
            } else {
                Preprocess(full_path, out_file, include_directories);                
            }
        } else if (regex_match(line, file_name, incl_std_reg)) {
            path p(file_name[1]);
            const auto search_result = 
            SearchInIncludeDirectories(include_directories, p);
            if (search_result == include_directories.end()) {                 
                cout << "unknown include file "s << p.filename().string() 
                     << " at file "s << in_file.string() 
                     << " at line "s << line_number << endl;
                return false;
            } 
            full_path = *search_result / p;
            Preprocess(full_path, out_file, include_directories);
        } else {
            output_file << line << endl;   
        }
    }
    
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);
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

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p, {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

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

    cout << endl << GetFileContents("sources/a.in"s) << endl;
    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
