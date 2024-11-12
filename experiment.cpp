#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

using namespace std;
const int KB_TO_MB = 1024;
const int BOTTOM_TRACKS = 10241; // bottom tracks 20GB, 下比上多一比較好計算
const int TOP_TRACKS = 10240;    // top tracks 20GB
const int INDEX = 320;           // 10240/32=320, 用於簡化紀錄在track上的level,key
// 函式: 讀取檔案並將資料分別儲存到兩個陣列中
void readSSTableFile(const string &filename, vector<int> &level, vector<int> &key)
{
    ifstream infile(filename);

    if (!infile.is_open())
    {
        cerr << "無法開啟檔案" << endl;
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
void extract_four_sstable(vector<int> &level, vector<int> &key, int index, vector<int> &allocat_level, vector<int> &allocat_key)
{
    int i = 0;
    for (i = 0; i < 4; i++)
    {
        allocat_level[i] = level[index];
        allocat_key[i] = key[index];
        index = index + 1;
    }
}
int judge_level(int &level)
{
    if (level == 4)
        return 4;
    else
        return 0;
}
void allocate_SStable(vector<int> &allocat_level, vector<int> &allocat_key, vector<int> &top_tracks, vector<int> &bottom_tracks, vector<int> &top_sstable_level, vector<int> &bottom_sstable_level, vector<int> &top_sstable_key, vector<int> &bottom_sstable_key)
{
    int i = 0, level = 0, top_space = 0;
    for (i = 0; i < 4; i++) // 4張sstable
    {
        // judge level
        level = judge_level(allocat_level[i]); // 1 = level4
        // judge top tracks spaces
        if (top_tracks[10239] == 0)
        {
            top_space = 1; // 1=have space, 0=no space
        }
        else
        {
            top_space = 0;
        }

        if (level == 4 && top_space == 1)
        {
            // judge key
            // write to top
            // cout << "4" << endl;
        }
        else
        {
            // judge key
            // write to bottom
            // cout << "except 4" << endl;
        }
    }
}
int main()
{
    vector<int> level;
    vector<int> key;
    vector<int> allocat_level(4);             // 提取4個level
    vector<int> allocat_key(4);               // 提取4個key
    vector<int> top_tracks(TOP_TRACKS);       // 紀錄top track被使用情形
    vector<int> bottom_tracks(BOTTOM_TRACKS); // 紀錄bottom track被使用情形
    // track上存的level
    vector<int> top_sstable_level(INDEX);
    vector<int> bottom_sstable_level(INDEX);
    // track上存的key
    vector<int> top_sstable_key(INDEX);
    vector<int> bottom_sstable_key(INDEX);

    int i = 0;
    // 呼叫函式讀取檔案，並將結果存入 level 和 key 陣列中
    readSSTableFile("sstable_info_0.1.txt", level, key);

    for (i = 0; i < 480; i += 4)
    {
        extract_four_sstable(level, key, i, allocat_level, allocat_key); // 提取完4個要寫入sstable
        allocate_SStable(allocat_level, allocat_key, top_tracks, bottom_tracks, top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key);
    }

    return 0;
}
