#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
using namespace std;
const int KB_TO_MB = 1024;
const int BOTTOM_TRACKS = 10240;                                                     // bottom tracks 20GB
const int TOP_TRACKS = 10240;                                                        // top tracks 20GB
void read_sstable_info(const string &filename, vector<int> &level, vector<int> &key) // load sstable info. to matrix
{
    ifstream infile(filename);

    if (!infile.is_open())
    {
        cerr << "no file" << endl;
        return;
    }
    string line;
    while (getline(infile, line))
    {
        stringstream ss(line);
        string temp;
        int l, k;

        // 讀取逗號前的第一個數字 (level)
        if (getline(ss, temp, ','))
        {
            l = stoi(temp);     // 將字串轉換為整數
            level.push_back(l); // 儲存到 level 陣列
        }

        // 讀取逗號後的第二個數字 (key)
        if (getline(ss, temp, ','))
        {
            k = stoi(temp);   // 將字串轉換為整數
            key.push_back(k); // 儲存到 key 陣列
        }
    }

    infile.close();
}
int main(void)
{
    vector<int> level;
    vector<int> key;
    // read_sstable_info("sstable_info_0.1", level, key); //load info.

    return 0;
}
