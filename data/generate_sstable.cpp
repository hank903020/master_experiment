#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <sstream>
#include <ctime>

using namespace std;

// 產生 SSTable 資訊的函數
void generateSSTableInfo(float x)
{
    stringstream ss;
    ss << x;                                               // 將浮點數 x 輸入字串流
    string filename = "sstable_info_" + ss.str() + ".txt"; // 組合成檔案名稱

    // 開啟輸出的.txt檔案
    ofstream outfile(filename);

    // 確認檔案已開啟
    if (!outfile.is_open())
    {
        cerr << "cant not open file" << endl;
        return;
    }

    // 計算應產生的Key數量和重複數量
    int total_sstables = 480;
    int unique_keys = total_sstables - static_cast<int>(total_sstables * x); // 432個唯一的Key
    int duplicate_keys = total_sstables - unique_keys;                       // 48個重複的Key

    // 生成1-432的唯一鍵
    vector<int> keys(unique_keys);
    for (int i = 0; i < unique_keys; ++i)
    {
        keys[i] = i + 1;
    }

    // 隨機分配唯一鍵
    unsigned seed;
    seed = (unsigned)(time(0));
    mt19937 g(seed);
    shuffle(keys.begin(), keys.end(), g);

    // 將多餘的48個鍵隨機重複到keys中
    for (int i = 0; i < duplicate_keys; ++i)
    {
        keys.push_back(keys[i]);
    }

    // 再次隨機分配所有鍵
    shuffle(keys.begin(), keys.end(), g);

    // 記錄level各自的SSTable數量
    int level4_count = 432;
    int level3_count = 44;
    int level2_count = 4;

    int key_index = 0;

    // 每次輸出4張SSTable
    for (int i = 0; i < 108; ++i)
    {
        // 每輸出10次level4就會輸出1次level3，計數器
        if (i % 10 == 0 && i > 0)
        {
            for (int j = 0; j < 4; ++j)
            {
                outfile << "3," << keys[key_index++] << endl;
                --level3_count;
            }

            // 每輸出10次level3就輸出1次level2，計數器
            if (i % 100 == 0 && i > 0)
            {
                for (int j = 0; j < 4; ++j)
                {
                    outfile << "2," << keys[key_index++] << endl;
                    --level2_count;
                }
            }
        }

        // 輸出level4的SSTable資訊
        for (int j = 0; j < 4; ++j)
        {
            outfile << "4," << keys[key_index++] << endl;
            --level4_count;
        }
    }

    // 輸出剩餘的level4 32張
    while (level4_count > 0)
    {
        outfile << "4," << keys[key_index++] << endl;
        --level4_count;
    }

    // 輸出剩餘的level3 4張
    while (level3_count > 0)
    {
        outfile << "3," << keys[key_index++] << endl;
        --level3_count;
    }

    // 關閉檔案
    outfile.close();
}

int main()
{
    float x;
    cout << "enter x (0.1=10%): ";
    cin >> x;

    // 生成SSTable資訊
    generateSSTableInfo(x);

    cout << "SSTable information output to sstable_info.txt" << endl;
    return 0;
}
