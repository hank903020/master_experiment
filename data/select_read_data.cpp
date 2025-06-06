#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <ctime>

using namespace std;

// 資料結構：一筆資料為 (level, key)
struct SSTableEntry
{
    int level;
    int key;
};

// 讀取檔案並儲存所有 (level, key)
vector<SSTableEntry> readSSTableFile(const string &filename)
{
    ifstream infile(filename);
    vector<SSTableEntry> data;

    if (!infile.is_open())
    {
        cerr << "無法開啟檔案：" << filename << endl;
        return data;
    }

    string line;
    while (getline(infile, line))
    {
        stringstream ss(line);
        string temp;
        int level, key;

        if (getline(ss, temp, ','))
        {
            level = stoi(temp);
        }
        if (getline(ss, temp, ','))
        {
            key = stoi(temp);
        }
        data.push_back({level, key});
    }

    infile.close();
    return data;
}

// 隨機選擇 48 筆資料
vector<SSTableEntry> selectRandomEntries(const vector<SSTableEntry> &data, int count)
{
    vector<SSTableEntry> shuffled = data;

    // 使用亂數種子
    unsigned seed;
    seed = (unsigned)(time(0));
    mt19937 g(seed);
    shuffle(shuffled.begin(), shuffled.end(), g);

    // 取前 count 筆
    vector<SSTableEntry> selected(shuffled.begin(), shuffled.begin() + count);
    return selected;
}

// 輸出結果至檔案
void writeSelectedEntries(const string &outputFile, const vector<SSTableEntry> &entries)
{
    ofstream outfile(outputFile);
    if (!outfile.is_open())
    {
        cerr << "無法開啟輸出檔案：" << outputFile << endl;
        return;
    }

    for (const auto &entry : entries)
    {
        outfile << entry.level << "," << entry.key << endl;
    }

    outfile.close();
    cout << "已輸出至：" << outputFile << endl;
}

int main()
{
    // 自定義輸入的 x 值
    float x = 0.1;

    // 組合輸入檔案名稱 sstable_info_x.txt
    stringstream ss_in;
    ss_in << "sstable_info_" << x << ".txt";
    string inputFilename = ss_in.str();

    // 組合輸出檔案名稱 read_count_data_x.txt
    stringstream ss_out;
    ss_out << "read_count_data_" << x << ".txt";
    string outputFilename = ss_out.str();

    // 讀入所有資料
    vector<SSTableEntry> allData = readSSTableFile(inputFilename);

    if (allData.size() < 48)
    {
        cerr << "錯誤：資料筆數不足 48 筆，僅有 " << allData.size() << " 筆。" << endl;
        return 1;
    }

    // 隨機挑選 48 筆
    vector<SSTableEntry> selected = selectRandomEntries(allData, 48);

    // 輸出結果到 read_count_data_x.txt
    writeSelectedEntries(outputFilename, selected);

    return 0;
}
