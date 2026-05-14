
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <ctime>
using namespace std;

static string _ts(long long v){ostringstream o;o<<v;return o.str();}
static string _ts(double v)   {ostringstream o;o<<v;return o.str();}
static string _ts(bool v)     {return v?"TRUE":"FALSE";}
static string _ts(char v)     {return string(1,v);}
static string _ts(string v)   {return v;}
template<typename T>
static string _ts(vector<T> v){return "[array]";}
static string _ucase(string s){transform(s.begin(),s.end(),s.begin(),::toupper);return s;}
static string _lcase(string s){transform(s.begin(),s.end(),s.begin(),::tolower);return s;}


int main(){
    srand((unsigned)time(nullptr));
    string name="";
    long long age=0LL;
    cout << _ts(string("请输入你的名字：")) << "\n";
    cin >> name;
    cout << _ts(string("请输入你的年龄：")) << "\n";
    cin >> age;
    if((age>=18LL)){
        cout << _ts((_ts((_ts((_ts(name)+_ts(string(" 已成年（"))))+_ts(age)))+_ts(string(" 岁）")))) << "\n";
    } else {
        cout << _ts((_ts((_ts((_ts(name)+_ts(string(" 未成年（"))))+_ts(age)))+_ts(string(" 岁）")))) << "\n";
    }
    return 0;
}