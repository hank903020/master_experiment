#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
using namespace std;

const int BOTTOM_TRACKS = 10240; // bottom tracks 20GB
const int TOP_TRACKS = 10240;    // top tracks 20GB
const int INDEX = 320;           // 10240/32=320, 用於簡化紀錄在track上的level,key
const double t_seek_min = 2.0;   // 最小尋道時間，單位：毫秒
const double t_rotation = 10.0;  // 旋轉時間，單位：毫秒，假設為6000RPM，即一整圈10毫秒

// 計算尋道時間，t_seek, a = 0.125779
double calculateSeekTime(int track_src, int track_des)
{
    return 0.125779 * sqrt(2 * (abs(track_src - track_des))) + t_seek_min;
}

// 計算I/O延遲
double calculateIOLatency(int track_src, int track_des, int isRMW, bool top_or_bottom)
{
    double t_seek = calculateSeekTime(track_src, track_des);
    if (isRMW == 1) // 傳入1的話代表半個RMW
        return 32 * (t_seek + 4 * t_rotation);
    else if (isRMW == 2)
        return 32 * (t_seek + 6 * t_rotation);
    else
    {
        if (top_or_bottom) // 1 = write bottom latency
            return 32 * (t_seek + 0.5 * t_rotation);
        else // write top latency
            return 64 * (t_seek + 0.5 * t_rotation);
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

// 判斷level and key，先判斷key的存在，在判斷是否存在同個level，考慮覆寫情況
int judge_overwrite(int &level, int &key, vector<int> &top_sstable_level, vector<int> &bottom_sstable_level, vector<int> &top_sstable_key, vector<int> &bottom_sstable_key, int &index_position)
{
    int i = 0;
    // judge bottom
    for (i = 0; i < INDEX; i++)
    {
        if (key == bottom_sstable_key[i]) // 先判斷是否有一樣的key
        {
            if (level == bottom_sstable_level[i]) // 在判斷是否有一樣的level
            {
                index_position = i;
                return 1; // 1 代表存在在bottom上
            }
        }
    }

    // judge top
    for (i = 0; i < INDEX; i++)
    {
        if (key == top_sstable_key[i])
        {
            if (level == top_sstable_level[i])
            {
                if (i % 2 == 0) // 偶數
                {
                    index_position = i;
                    return 2;
                }
                else // 奇數
                {
                    index_position = i;
                    return 3;
                }
            }
        }
    }

    return 0; // no exist
}

// write bottom
void write_bottom(vector<int> &bottom_tracks, int bottom_flag)
{
    int i = 0;
    for (i = 0; i < 32; i++)
    {
        bottom_tracks[bottom_flag] = 1;
        bottom_flag = bottom_flag + 1;
    }
}
// record bottom level and key
void Record_bottom_sstable(vector<int> &bottom_sstable_level, vector<int> &bottom_sstable_key, int &level, int &key, int flag)
{
    int index = flag / 32;
    bottom_sstable_level[index] = level;
    bottom_sstable_key[index] = key;
}

// write top
void write_top(vector<int> &top_tracks, int top_flag)
{
    int i = 0;
    for (i = 0; i < 32; i++)
    {
        top_tracks[top_flag] = 1;
        top_flag = top_flag + 2;
    }
}
// record top level and key
void Record_top_sstable(vector<int> &top_sstable_level, vector<int> &top_sstable_key, int &level, int &key, int flag, bool even_or_odd)
{
    int index = 0;
    if (even_or_odd == 0) // even
    {
        index = flag / 32;
        top_sstable_level[index] = level;
        top_sstable_key[index] = key;
    }
    else
    {
        index = flag / 32;
        index = index + 1;
        top_sstable_level[index] = level;
        top_sstable_key[index] = key;
    }
}

// isRmw
int judge_RMW(vector<int> &top_sstable_level, int index_position)
{
    int i = index_position;
    if (i == 0)
    {
        if (top_sstable_level[i] == 0 && top_sstable_level[i + 1] == 0)
            return 0;
        else if (top_sstable_level[i] == 0 || top_sstable_level[i + 1] == 0)
            return 1;
        else
            return 2;
    }
    else
    {
        if (top_sstable_level[i] == 0 && top_sstable_level[i - 1] == 0)
            return 0;
        else if (top_sstable_level[i] == 0 || top_sstable_level[i - 1] == 0)
            return 1;
        else
            return 2;
    }
}

void allocate_SStable(double &latency, int &top_overwrite, int &track_sector, int &top_flag, int &bottom_flag, vector<int> &allocat_level, vector<int> &allocat_key, vector<int> &top_tracks, vector<int> &bottom_tracks, vector<int> &top_sstable_level, vector<int> &bottom_sstable_level, vector<int> &top_sstable_key, vector<int> &bottom_sstable_key)
{
    int i = 0, bottom_space = 0, overwrite = 0,
        index_position = 0;   // 定位sstable and key index
    int sstable_position = 0; // 換算index to track sector
    int isRMW = 0;

    for (i = 0; i < 4; i++) // 4張sstable
    {
        // judge bottom space
        if (bottom_tracks[10239] == 0)
            bottom_space = 1; // 1=have space, 0=no space
        else
            bottom_space = 0;

        // judge overwrite
        overwrite = judge_overwrite(allocat_level[i], allocat_key[i], top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key, index_position);

        if (overwrite == 1) // overwrite bottom
        {
            // position bottom index
            sstable_position = (index_position * 32); // 還原sstable在track上的位置，為sstable起點
            // judge RMW
            isRMW = judge_RMW(top_sstable_level, index_position);
            // caculate top overwrite
            if (isRMW == 2)
                top_overwrite = top_overwrite + 32;
            else if (isRMW == 1)
                top_overwrite = top_overwrite + 16;
            else
                top_overwrite = top_overwrite;
            // caculate track distance and write latency
            // 紀錄sector移動到哪裡
            track_sector = sstable_position;
            track_sector = track_sector + 31;
        }
        else if (overwrite == 2) // overwrite top even
        {
            // position top index
            sstable_position = (index_position * 32); // sstable的起點sector
            // caculate track distance and write latency
            // 紀錄sector移動到哪裡
            track_sector = sstable_position + 62;
        }
        else if (overwrite == 3) // overwrite top odd
        {
            // position top index
            sstable_position = (index_position * 32) - 31; // sstable的起點sector
            // caculate track distance and write latency
            // 紀錄sector移動到哪裡
            track_sector = sstable_position + 62;
        }
        else // write new track
        {
            if (bottom_space) // bottom have space，優先存bottom不會有RMW問題，overwrite時才會有
            {
                // write bottom
                write_bottom(bottom_tracks, bottom_flag);
                // 紀錄sstable and key info.
                Record_bottom_sstable(bottom_sstable_level, bottom_sstable_key, allocat_level[i], allocat_key[i], bottom_flag);
                // caculate write latency, use track_sector and bottom_flag
                // 紀錄sector移動到哪裡
                if (bottom_flag == 0)
                    track_sector = track_sector + 31;
                else
                    track_sector = track_sector + 32;
                // 最後定位bottom flag到哪裡
                bottom_flag = bottom_flag + 32;
            }
            else // store on top
            {
                if (top_sstable_level[318] == 0) // even
                {
                    // write top
                    write_top(top_tracks, top_flag);
                    // 紀錄sstable and key info.
                    Record_top_sstable(top_sstable_level, top_sstable_key, allocat_level[i], allocat_key[i], top_flag, 0);
                    // caculate write latency, use track_sector and top_flag
                    // 紀錄sector移動到哪裡
                    track_sector = track_sector + 62;
                    // 最後定位top flag到哪裡
                    top_flag = top_flag + 64;
                }
                else // odd
                {
                    // judge first time write to odd
                    if (top_sstable_level[1] == 0)
                        top_flag = 1;
                    // write top
                    write_top(top_tracks, top_flag);
                    // 紀錄sstable and key info.
                    Record_top_sstable(top_sstable_level, top_sstable_key, allocat_level[i], allocat_key[i], top_flag, 1);
                    // caculate write latency, use track_sector and top_flag
                    // 紀錄sector移動到哪裡
                    track_sector = track_sector + 62;
                    // 最後定位top flag到哪裡
                    top_flag = top_flag + 64;
                }
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
    int top_flag = 0, bottom_flag = 0; // record top and bottom track store where
    int track_sector = 0;              // sector position
    double latency = 0;                // write latency
    int top_overwrite = 0;             // 紀錄top複寫次數

    int i = 0;
    // 呼叫函式讀取檔案，並將結果存入 level 和 key 陣列中
    readSSTableFile("sstable_info_0.1.txt", level, key);

    for (i = 0; i < 480; i += 4)
    {
        extract_four_sstable(level, key, i, allocat_level, allocat_key); // 提取完4個要寫入sstable
        allocate_SStable(latency, top_overwrite, track_sector, top_flag, bottom_flag, allocat_level, allocat_key, top_tracks, bottom_tracks, top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key);
    }
}
