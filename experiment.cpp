#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

using namespace std;
const int KB_TO_MB = 1024;
const int BOTTOM_TRACKS = 10241; // bottom tracks 20GB, 下比上多一比較好計算
const int TOP_TRACKS = 10240;    // top tracks 20GB
const int INDEX = 320;           // 10240/32=320, 用於簡化紀錄在track上的level,key
const double t_seek_min = 2.0;   // 最小尋道時間，單位：毫秒
const double t_rotation = 10.0;  // 旋轉時間，單位：毫秒，假設為6000RPM，即一整圈10毫秒

// 計算尋道時間，t_seek, a = 0.125779
double calculateSeekTime(int track_src, int track_des)
{
    return 0.125779 * sqrt(abs(track_src - track_des)) + t_seek_min;
}

// 計算I/O延遲
double calculateIOLatency(int track_src, int track_des, bool isRMW)
{
    double t_seek = calculateSeekTime(track_src, track_des);
    if (isRMW)
    {
        return 32 * (t_seek + 6 * t_rotation);
    }
    else
    {
        return 32 * (t_seek + 0.5 * t_rotation);
    }
}

// 讀取檔案並將資料分別儲存到兩個陣列中
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

bool judge_level(int &level)
{
    if (level == 4)
        return 1;
    else
        return 0;
}

// 判斷level and key，先判斷key的存在，在判斷是否存在同個level，考慮覆寫情況
int judge_overwrite(int &level, int &key, vector<int> &top_sstable_level, vector<int> &bottom_sstable_level, vector<int> &top_sstable_key, vector<int> &bottom_sstable_key, int &index_position)
{
    int i = 0;

    // judge key(top)
    for (i = 0; i < INDEX; i++)
    {
        if (key == top_sstable_key[i]) // 先判斷是否有一樣的key
        {
            if (level == top_sstable_level[i]) // 在判斷是否有一樣的level
            {
                index_position = i;
                return 1; // 1 代表存在在top上
            }
        }
    }
    // judge key(bottom)
    for (i = 319; i < 0; i--)
    {
        if (key == bottom_sstable_key[i]) // 先判斷是否有一樣的key
        {
            if (level == bottom_sstable_level[i]) // 在判斷是否有一樣的level
            {
                index_position = i;
                return 2; // 2 代表存在在bottom上
            }
        }
    }
    return 0; // 不存在
}

void allocate_SStable(int &track_sector, vector<int> &allocat_level, vector<int> &allocat_key, vector<int> &top_tracks, vector<int> &bottom_tracks, vector<int> &top_sstable_level, vector<int> &bottom_sstable_level, vector<int> &top_sstable_key, vector<int> &bottom_sstable_key)
{
    int i = 0, overwrite = 0, top_space = 0, level = 0,
        index_position = 0;   // 定位index
    int sstable_position = 0; // 換算index to track sector

    for (i = 0; i < 4; i++) // 4張sstable
    {
        // judge top tracks spaces
        if (top_tracks[10239] == 0)
            top_space = 1; // 1=have space, 0=no space
        else
            top_space = 0;

        // judge 是否覆寫
        overwrite = judge_overwrite(allocat_level[i], allocat_key[i], top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key, index_position);
        if (overwrite == 1) // overwrite on top
        {
            // position top index
            sstable_position = (index_position * 32); // 還原sstable track位置，為sstable起點
            // caculate track distance and write latency
        }
        else if (overwrite == 2) // overwrite on bottom
        {
            // position bottom index
            sstable_position = (index_position * 32); // 還原sstable track位置，為sstable終點
            // judge RMW and caculate track distance and write latency
        }
        else // write on new tracks
        {
            level = judge_level(allocat_level[i]);
            if (level == 1 && top_space == 1) // level 4 && top have space
            {
                // write top
            }
            else
            {
                // write bottom
            }
        }
    }
}
int main(void)
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
    int track_sector = 0; // sector position
    double latency = 0;

    int i = 0;
    // 呼叫函式讀取檔案，並將結果存入 level 和 key 陣列中
    readSSTableFile("sstable_info_0.1.txt", level, key);

    for (i = 0; i < 480; i += 4)
    {
        extract_four_sstable(level, key, i, allocat_level, allocat_key); // 提取完4個要寫入sstable
        allocate_SStable(track_sector, allocat_level, allocat_key, top_tracks, bottom_tracks, top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key);
    }
}
