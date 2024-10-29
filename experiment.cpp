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
void extract_four_sstable(vector<int> &level, vector<int> &key, int index, vector<int> &allocat_level, vector<int> &allocat_key) // extract four sstable
{
    int i = 0;
    for (i = 0; i < 4; i++)
    {
        allocat_level[i] = level[index];
        allocat_key[i] = key[index];
        index = index + 1;
    }
}
int main(void)
{
    vector<int> level;            // index 0-479
    vector<int> key;              // index 0-479
    vector<int> allocat_level(4); // 提取4個level
    vector<int> allocat_key(4);   // 提取4個key
    // read_sstable_info("sstable_info_0.1", level, key); //load info.
    int i = 0; // extract four sstable index
    /*for (i = 0; i < 480; i += 4)  //extract four sstable
        extract_four_sstable(level, key, i, allocat_level, allocat_key);*/

    return 0;
}
