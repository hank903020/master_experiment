#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>

using namespace std;
const int BOTTOM_TRACKS = 10241; // bottom tracks 20GB, 下比上多一比較好計算
const int TOP_TRACKS = 10240;    // top tracks 20GB
const int INDEX = 320;           // 10240/32=320, 用於簡化紀錄在track上的level,key
const double t_seek_min = 2.0;   // 最小尋道時間，單位：毫秒
const double t_rotation = 10.0;  // 旋轉時間，單位：毫秒，假設為6000RPM，即一整圈10毫秒
int more_sstable = 0;

// 計算尋道時間，t_seek, a = 0.125779
double calculateSeekTime(int track_src, int track_des)
{
    return 0.125779 * sqrt(2 * (abs(track_src - track_des))) + t_seek_min;
}

// 計算I/O延遲
double calculateIOLatency(int track_src, int track_des, bool isRMW)
{
    double t_seek = calculateSeekTime(track_src, track_des);
    if (isRMW) // 傳入1的話代表RMW
        return 32 * (t_seek + 6 * t_rotation);
    else
        return 32 * (t_seek + 0.5 * t_rotation);
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
    int index = 0; // 用來追蹤目前存入的位置

    while (getline(infile, line) && index < 480)
    { // 確保不超過 vector 的大小
        stringstream ss(line);
        string temp;

        // 讀取逗號前的第一個數字 (level)
        if (getline(ss, temp, ','))
        {
            level[index] = stoi(temp); // 將字串轉換為整數並存入 level
        }

        // 讀取逗號後的第二個數字 (key)
        if (getline(ss, temp, ','))
        {
            key[index] = stoi(temp); // 將字串轉換為整數並存入 key
        }

        index++; // 更新索引
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

// wirte top
void Write_top(vector<int> &top_tracks, int top_flag)
{
    int i = 0;
    for (i = 0; i < 32; i++)
    {
        top_tracks[top_flag] = 1;
        top_flag = top_flag + 1;
    }
}

// wirte bottom
void Write_bottom(vector<int> &bottom_tracks, int bottom_flag)
{
    int i = 0;
    for (i = 0; i < 32; i++)
    {
        bottom_tracks[bottom_flag] = 1;
        bottom_flag = bottom_flag - 1;
    }
}

// record top sstable level and key
void Record_top_sstable(vector<int> &top_sstable_level, vector<int> &top_sstable_key, int &level, int &key, int flag)
{
    int index = flag / 32;
    top_sstable_level[index] = level;
    top_sstable_key[index] = key;
}

// record bottom sstable level and key
void Record_bottom_sstable(vector<int> &bottom_sstable_level, vector<int> &bottom_sstable_key, int &level, int &key, int flag)
{
    int index = flag / 32;
    index = index - 1; // 不同於record top 是因為我把bottom多設一個track，會導致10240/32=320，但是array index只到319，必須減一。
    bottom_sstable_level[index] = level;
    bottom_sstable_key[index] = key;
}

bool judge_level(int &level)
{
    if (level == 4)
        return 1;
    else
        return 0;
}

bool judge_RMW(vector<int> &top_sstable_level, int index_position)
{
    if (top_sstable_level[index_position] == 0)
        return 0;
    else
        return 1;
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

void allocate_SStable(double &latency, double &WAF, int &top_overwrite, int &track_sector, int &top_flag, int &bottom_flag, vector<int> &allocat_level, vector<int> &allocat_key, vector<int> &top_tracks, vector<int> &bottom_tracks, vector<int> &top_sstable_level, vector<int> &bottom_sstable_level, vector<int> &top_sstable_key, vector<int> &bottom_sstable_key)
{
    int i = 0, overwrite = 0, top_space = 0, level = 0, bottom_space = 0,
        index_position = 0;   // 定位sstable and key index
    int sstable_position = 0; // 換算index to track sector

    bool isRMW = 0;

    for (i = 0; i < 4; i++) // 4張sstable
    {
        // judge top tracks spaces
        if (top_tracks[10239] == 0)
            top_space = 1; // 1=have space, 0=no space
        else
            top_space = 0;
        // judge bottom track space
        if (bottom_tracks[0] == 0)
            bottom_space = 1; // 1=have space, 0=no space
        else
            bottom_space = 0;

        // judge 是否覆寫
        overwrite = judge_overwrite(allocat_level[i], allocat_key[i], top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key, index_position);
        if (overwrite == 1) // overwrite on top
        {
            // position top index
            sstable_position = (index_position * 32); // 還原sstable track位置，為sstable起點
            // caculate track distance and write latency
            latency = latency + calculateIOLatency(track_sector, sstable_position, 0);
            // 紀錄sector移動到哪裡
            track_sector = sstable_position;
            track_sector = track_sector + 31;
        }
        else if (overwrite == 2) // overwrite on bottom
        {
            // position bottom index
            sstable_position = (index_position * 32); // 還原sstable track位置，為sstable終點的下一個 //index_position在judge_overwrite裡面抓出來
            sstable_position = sstable_position + 32; // 抓出sstable起點位置
            // judge RMW
            isRMW = judge_RMW(top_sstable_level, index_position);
            // caculate top overwrite
            if (isRMW)
            {
                top_overwrite = top_overwrite + 32;
                WAF = WAF + 64;
            }

            // caculate track distance and write latency
            latency = latency + calculateIOLatency(track_sector, sstable_position, isRMW);
            // 紀錄sector移動到哪裡
            track_sector = sstable_position - 31;
        }
        else // write on new tracks
        {
            level = judge_level(allocat_level[i]);
            if (level == 1 && top_space == 1) // level 4 && top have space
            {
                // write top
                Write_top(top_tracks, top_flag);
                // 紀錄sstable level and key
                Record_top_sstable(top_sstable_level, top_sstable_key, allocat_level[i], allocat_key[i], top_flag);
                // caculate write latency, use track_sector and top_flag
                latency = latency + calculateIOLatency(track_sector, top_flag, 0);
                // 紀錄sector移動到哪裡
                if (top_flag == 0)
                    track_sector = track_sector + 31;
                else
                    track_sector = track_sector + 32;
                // 最後定位top track目前存到哪裡
                top_flag = top_flag + 32;
            }
            else
            {
                if (bottom_space == 1) // 多次輸入時，當bottom空間夠時才執行
                {
                    // write bottom
                    Write_bottom(bottom_tracks, bottom_flag);
                    // 紀錄sstable level and key
                    Record_bottom_sstable(bottom_sstable_level, bottom_sstable_key, allocat_level[i], allocat_key[i], bottom_flag);
                    // judge RMW
                    if (top_flag > bottom_flag) // 代表交叉
                        isRMW = 1;
                    else
                        isRMW = 0;
                    // caculate top overwrite
                    if (isRMW)
                    {
                        top_overwrite = top_overwrite + 32;
                        WAF = WAF + 64;
                    }

                    // caculate track distance and write latency, use track_sector and bottom_flag
                    latency = latency + calculateIOLatency(track_sector, bottom_flag, isRMW);
                    // 紀錄sector移動到哪裡
                    if (bottom_flag == 10240)
                        track_sector = track_sector - 31;
                    else
                        track_sector = track_sector - 32;
                    // 最後定位bottom track目前存到哪裡
                    bottom_flag = bottom_flag - 32;
                }
                else
                {
                    more_sstable += 1; // 多次輸入時，計算多餘sstable
                }
            }
        }
    }
}

// outout
void write_to_output(const string &filename, double &latency, double &WAF, int &top_overwrite, int &top_flag, int &bottom_flag, int i)
{
    ofstream outfile(filename, ios::app); // 開啟檔案
    if (!outfile.is_open())               // 檢查是否成功開啟
    {
        cerr << "Error: Unable to open file " << filename << endl;
        return;
    }
    // 換算GB
    double ii = i;
    int GB = 10;
    ii = ii / 160;
    GB = GB * ii;

    // 計算目前真正使用大小 單位為MB
    int use_top = 0, use_bottom = 0, total = 0;
    use_top = (top_flag) * 2;
    use_bottom = (10240 - bottom_flag) * 2;
    total = use_top + use_bottom; // MB

    // output
    outfile << "GB: " << GB << endl;
    outfile << "actual use: " << total << "MB" << endl;
    outfile << "latency: " << latency << "ms" << endl;
    outfile << "top: " << top_flag << " " << "bottom: " << bottom_flag << endl;
    outfile << "top overwrite: " << top_overwrite << endl;
    if (i == 480)
    {
        double waf = 0;
        waf = WAF / 30720;
        outfile << "write amplification factor: " << waf << endl;
    }
    outfile << endl;

    outfile.close();
}

void initialization(vector<int> &level, vector<int> &key, double &latency, int &top_overwrite, double &WAF)
{
    level.clear();
    level.resize(480, 0);
    key.clear();
    key.resize(480, 0);
    latency = 0;
    top_overwrite = 0;
    WAF = 30720;
}

// *****************計算read latency******************
// 計算read latency
double calculateReadLatency(int track_src, int track_des)
{
    double t_seek = calculateSeekTime(track_src, track_des);
    return t_seek;
}
void calculateReadLatency(const string &read_order_file, const vector<int> &top_sstable_level, const vector<int> &bottom_sstable_level, const vector<int> &top_sstable_key, const vector<int> &bottom_sstable_key, double &read_latency_total, int &current_sector) // 追蹤磁頭目前位置
{
    ifstream infile(read_order_file);
    if (!infile.is_open())
    {
        cerr << "無法開啟讀取順序檔案：" << read_order_file << endl;
        return;
    }

    string line;
    while (getline(infile, line))
    {
        stringstream ss(line);
        string temp;
        int level = 0, key = 0;

        if (getline(ss, temp, ','))
            level = stoi(temp);
        if (getline(ss, temp, ','))
            key = stoi(temp);

        // 在 top 搜尋
        bool found = false;
        int sstable_start = 0;
        for (int i = 0; i < INDEX; ++i)
        {
            if (top_sstable_level[i] == level && top_sstable_key[i] == key)
            {
                sstable_start = i * 32; // Top 的起始位置
                found = true;
                break;
            }
        }

        // 在 bottom 搜尋
        if (!found)
        {
            for (int i = 0; i < INDEX; ++i)
            {
                if (bottom_sstable_level[i] == level && bottom_sstable_key[i] == key)
                {
                    sstable_start = i * 32;
                    break;
                }
            }
        }

        // 計算latency
        read_latency_total = read_latency_total + calculateReadLatency(current_sector, sstable_start);
        // 更新目前磁頭位置
        current_sector = sstable_start + 31;
    }

    infile.close();
}
void writeReadLatencyOutput(const string &output_file, double read_latency_total) // read latency outputs
{
    ofstream outfile(output_file);
    if (!outfile.is_open())
    {
        cerr << "無法開啟輸出檔案：" << output_file << endl;
        return;
    }

    outfile << "Total Read Track Movement: " << read_latency_total << " ms" << endl;
    outfile.close();
}

int main(void)
{
    vector<int> level(480);                   // 讀取存入vector
    vector<int> key(480);                     // 讀取存入vector
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
    int track_sector = 0;                  // sector position
    int top_flag = 0, bottom_flag = 10240; // record top and bottom track store where
    double latency = 0;                    // write latency
    int top_overwrite = 0;                 // 紀錄top複寫次數
    double WAF = 30720;                    // 計算寫入放大

    float x = 0.3; // 修改此變數，產出不同%數data
    stringstream ss;
    ss << "sstable_info_" << x << ".txt";
    string filename = ss.str(); // first load

    stringstream ss1;
    ss1 << "sstable_info_" << x << ".1.txt";
    string filename1 = ss1.str(); // second load

    stringstream ss2;
    ss2 << "output_file_" << x << "x2.txt";
    string filenamex2 = ss2.str(); // double load data use

    stringstream ss3;
    ss3 << "sstable_info_" << x << ".2.txt";
    string filename2 = ss3.str(); // third load

    stringstream ss4;
    ss4 << "output_file_" << x << "x3.txt"; // 0.1資料過多無法使用triple
    string filenamex3 = ss4.str();          // triple load data

    //********************************first**********************************************
    int i = 0;
    // 呼叫函式讀取檔案，並將結果存入 level 和 key 陣列中
    readSSTableFile(filename, level, key);

    for (i = 0; i < 480; i += 4)
    {
        extract_four_sstable(level, key, i, allocat_level, allocat_key); // 提取完4個要寫入sstable
        allocate_SStable(latency, WAF, top_overwrite, track_sector, top_flag, bottom_flag, allocat_level, allocat_key, top_tracks, bottom_tracks, top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key);
        //        if (i != 0 && i % 80 == 0) // 每5GB輸出一次資訊
        //            write_to_output(filenamex3, latency, WAF, top_overwrite, top_flag, bottom_flag, i);
    }
    //    write_to_output(filenamex3, latency, WAF, top_overwrite, top_flag, bottom_flag, i);
    //********************************first**********************************************

    // initialization
    initialization(level, key, latency, top_overwrite, WAF);

    //********************************second******************************************
    // 呼叫函式讀取檔案，並將結果存入 level 和 key 陣列中
    readSSTableFile(filename1, level, key);

    for (i = 0; i < 480; i += 4)
    {
        extract_four_sstable(level, key, i, allocat_level, allocat_key); // 提取完4個要寫入sstable
        allocate_SStable(latency, WAF, top_overwrite, track_sector, top_flag, bottom_flag, allocat_level, allocat_key, top_tracks, bottom_tracks, top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key);
        //        if (i != 0 && i % 80 == 0) // 每5GB輸出一次資訊
        //            write_to_output(filenamex3, latency, WAF, top_overwrite, top_flag, bottom_flag, i);
    }
    //    write_to_output(filenamex3, latency, WAF, top_overwrite, top_flag, bottom_flag, i);
    //********************************second******************************************

    // initialization
    initialization(level, key, latency, top_overwrite, WAF);

    //********************************third******************************************
    // 呼叫函式讀取檔案，並將結果存入 level 和 key 陣列中
    readSSTableFile(filename2, level, key);

    for (i = 0; i < 480; i += 4)
    {
        extract_four_sstable(level, key, i, allocat_level, allocat_key); // 提取完4個要寫入sstable
        allocate_SStable(latency, WAF, top_overwrite, track_sector, top_flag, bottom_flag, allocat_level, allocat_key, top_tracks, bottom_tracks, top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key);
        //        if (i != 0 && i % 80 == 0) // 每5GB輸出一次資訊
        //           write_to_output(filenamex3, latency, WAF, top_overwrite, top_flag, bottom_flag, i);
    }
    //    write_to_output(filenamex3, latency, WAF, top_overwrite, top_flag, bottom_flag, i);
    //********************************third******************************************

    //********************************Read latency caculate**************************
    double read_latency_total = 0;
    int current_sector = 0;

    float y = 0;
    y = x;
    stringstream rr;
    rr << "read_count_data_" << y << ".txt";
    string read_order = rr.str();
    stringstream rr2;
    rr2 << "read_latency_IADL_" << y << ".txt";
    string read_latency_info = rr2.str();

    calculateReadLatency(read_order, top_sstable_level, bottom_sstable_level, top_sstable_key, bottom_sstable_key, read_latency_total, current_sector);
    writeReadLatencyOutput(read_latency_info, read_latency_total);
}