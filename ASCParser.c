// Written by Google GEMINI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> // bool型を使用するために必要

// 定数定義
#define MAX_LINE_LENGTH 256
#define MAX_DATA_COUNT 8
#define LINES_PER_WRITE 30

// ヘッダー文字列
const char* HEADER = "time,ID,Phy,Dir,TA,PCI,SID,Data1,Data2,Data3,Data4,Data5,Data6,Data7,Data8\n";

// 関数プロトタイプ宣言
void print_help(const char* program_name);
char* get_output_filename(const char* input_filename);
void to_upper(char* str);

int main(int argc, char* argv[]) {
    // コマンドライン引数の処理
    if (argc == 1 || (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "?") == 0 || strcmp(argv[1], "-?") == 0))) {
        print_help(argv[0]);
        return 0;
    }

    const char* input_filepath = argv[1];
    char* output_filepath = NULL;

    if (argc == 2) {
        // 入力ファイルパスのみ指定された場合、出力ファイルパスを自動生成
        output_filepath = get_output_filename(input_filepath);
        if (output_filepath == NULL) {
            fprintf(stderr, "Error: Failed to generate output filename.\n");
            return 1;
        }
    } else if (argc == 3) {
        output_filepath = strdup(argv[2]); // コピーして使用 (解放が必要)
        if (output_filepath == NULL) {
            fprintf(stderr, "Error: Memory allocation failed for output filepath.\n");
            return 1;
        }
    } else {
        // 引数が多すぎる場合
        print_help(argv[0]);
        return 1;
    }

    FILE* infile = NULL;
    FILE* outfile = NULL;
    char line[MAX_LINE_LENGTH];
    char* parsed_data_buffer[LINES_PER_WRITE]; // 書き込みバッファ
    int buffer_count = 0;
    int line_num = 0;

    // 入力ファイルを開く
    infile = fopen(input_filepath, "r");
    if (infile == NULL) {
        fprintf(stderr, "Error: Could not open input file '%s'\n", input_filepath);
        free(output_filepath); // メモリ解放
        return 1;
    }

    // 出力ファイルを上書きモードで開く
    outfile = fopen(output_filepath, "w");
    if (outfile == NULL) {
        fprintf(stderr, "Error: Could not open output file '%s' for writing.\n", output_filepath);
        fclose(infile);
        free(output_filepath); // メモリ解放
        return 1;
    }

    // ヘッダーを書き込む
    fputs(HEADER, outfile);

    // ファイルを行ごとに読み込み、解析する
    while (fgets(line, sizeof(line), infile) != NULL) {
        line_num++;

        double time_value;
        char id_str_raw[32]; // ID文字列を一時的に格納
        unsigned int id_int_val; // IDの整数値
        char temp_rx[4], temp_d[2];
        int temp_num; // "d N" のN部分
        
        // Data部分を一時的に保持するためのバッファをlineから分離して使う
        // lineのコピーを作成し、Data部分を切り出す
        char line_copy[MAX_LINE_LENGTH];
        strncpy(line_copy, line, sizeof(line_copy) -1);
        line_copy[sizeof(line_copy) -1] = '\0'; // ヌル終端

        // Data部分の開始位置を特定する (例: " Rx   d 8 " の後)
        char* data_start_ptr = strstr(line_copy, " Rx   d ");
        if (data_start_ptr == NULL) {
            continue; // フォーマットに一致しない行
        }
        // " Rx   d 8 " の '8' の直後 (例: " Rx   d 8 " + strlen(" Rx   d ")) から Data を開始
        // ここで '8' の部分の桁数も考慮するため、さらに進める
        // まず " Rx   d " の次のスペースを見つける
        char* next_space_after_d = strchr(data_start_ptr + strlen(" Rx   d "), ' ');
        if (next_space_after_d == NULL) {
            continue; // フォーマットに一致しない行
        }
        // そのスペースの直後からDataが開始
        data_start_ptr = next_space_after_d + 1;


        unsigned int data_int_list[MAX_DATA_COUNT]; // Dataの整数値リスト
        char data_hex_list_formatted[MAX_DATA_COUNT][3]; // Dataの整形済み16進数文字列リスト ("XX\0")

        // 時刻とID、そして "Rx   d N" を解析
        // sscanfでIDの直後までを解析する
        int scanned_items = sscanf(line, "%lf %*d %s %s %s %d",
                                   &time_value, id_str_raw, temp_rx, temp_d, &temp_num);

        if (scanned_items < 5 || strcmp(temp_rx, "Rx") != 0 || strcmp(temp_d, "d") != 0) {
            continue; // フォーマットに一致しない行はスキップ
        }

        // ID文字列の整形: 'x'を取り除き大文字にする
        to_upper(id_str_raw);
        char* id_ptr = id_str_raw;
        if (strlen(id_ptr) > 0 && id_ptr[strlen(id_ptr) - 1] == 'X') {
            id_ptr[strlen(id_ptr) - 1] = '\0';
        }
        id_int_val = (unsigned int)strtol(id_ptr, NULL, 16); // 16進数として整数に変換

        // Data部分の解析 (空白で区切られた8つの2桁16進数)
        char* data_token = strtok(data_start_ptr, " \t\n"); // 最初のトークン
        int data_idx = 0;
        while (data_token != NULL && data_idx < MAX_DATA_COUNT) {
            if (strlen(data_token) == 2) { // 2桁の16進数であることを確認
                data_int_list[data_idx] = (unsigned int)strtol(data_token, NULL, 16);
                sprintf(data_hex_list_formatted[data_idx], "%02X", data_int_list[data_idx]);
                data_idx++;
            }
            data_token = strtok(NULL, " \t\n"); // 次のトークン
        }
        // Dataの数が8個に満たない場合は、残りを0で埋める
        for (; data_idx < MAX_DATA_COUNT; data_idx++) {
            data_int_list[data_idx] = 0;
            sprintf(data_hex_list_formatted[data_idx], "00");
        }

        // --- 追加列の計算 ---
        char phy_col[4] = "";
        char dir_col[4] = "";
        char ta_col[9] = ""; // IDが8桁の場合を考慮して長めに
        char pci_col[3] = "";
        char sid_col[3] = "";

        // Phy列
        if (id_int_val == 0x7DF || strncmp(id_ptr, "18DB", 4) == 0) {
            strcpy(phy_col, "0");
        } else if (id_int_val >= 0x7E0 && id_int_val <= 0x7E7) {
            strcpy(phy_col, "-1");
        } else {
            strcpy(phy_col, "1");
        }

        // Dir列
        if (strncmp(id_ptr, "18DAF1", 6) == 0 || (id_int_val >= 0x7E8 && id_int_val <= 0x7EF)) {
            strcpy(dir_col, "Res");
        } else {
            strcpy(dir_col, "Req");
        }

        // TA列
        size_t id_len = strlen(id_ptr);
        if (id_len == 8 && strncmp(id_ptr, "18", 2) == 0) {
            if (id_ptr[4] == 'F' && id_ptr[5] == '1') {
                strncpy(ta_col, id_ptr + 6, 2);
                ta_col[2] = '\0';
            } else if (id_ptr[6] == 'F' && id_ptr[7] == '1') {
                strncpy(ta_col, id_ptr + 4, 2);
                ta_col[2] = '\0';
            } else {
                ta_col[0] = '\0';
            }
        } else if (id_len == 3) {
            strcpy(ta_col, id_ptr);
        } else {
            ta_col[0] = '\0';
        }

        // PCI列 (Data1の上4bits)
        // Data1は常に存在すると仮定 (フォーマットの都合上)
        unsigned int data1_upper_4bits = (data_int_list[0] >> 4) & 0xF;
        if (data1_upper_4bits == 0) {
            strcpy(pci_col, "SF");
        } else if (data1_upper_4bits == 1) {
            strcpy(pci_col, "FF");
        } else if (data1_upper_4bits == 2) {
            strcpy(pci_col, "CF");
        } else if (data1_upper_4bits == 3) {
            strcpy(pci_col, "FC");
        } else {
            pci_col[0] = '\0';
        }

        // SID列
        if (strcmp(pci_col, "SF") == 0 && MAX_DATA_COUNT >= 2) { // Data2が存在することを確認
            sprintf(sid_col, "%02X", data_int_list[1]);
        } else if (strcmp(pci_col, "FF") == 0 && MAX_DATA_COUNT >= 3) { // Data3が存在することを確認
            sprintf(sid_col, "%02X", data_int_list[2]);
        } else {
            sid_col[0] = '\0';
        }

        // 出力行の文字列を作成し、バッファに格納
        char current_line_output[MAX_LINE_LENGTH * 2]; // 長さに余裕を持たせる
        snprintf(current_line_output, sizeof(current_line_output),
                 "%.6f,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
                 time_value,
                 id_ptr, // 整形済みID
                 phy_col, dir_col, ta_col, pci_col, sid_col,
                 data_hex_list_formatted[0], data_hex_list_formatted[1],
                 data_hex_list_formatted[2], data_hex_list_formatted[3],
                 data_hex_list_formatted[4], data_hex_list_formatted[5],
                 data_hex_list_formatted[6], data_hex_list_formatted[7]);

        // バッファにコピー (mallocして保存)
        parsed_data_buffer[buffer_count] = strdup(current_line_output);
        if (parsed_data_buffer[buffer_count] == NULL) {
            fprintf(stderr, "Error: Memory allocation failed for line buffer at line %d.\n", line_num);
            for(int i = 0; i < buffer_count; ++i) free(parsed_data_buffer[i]);
            fclose(infile);
            fclose(outfile);
            free(output_filepath);
            return 1;
        }
        buffer_count++;

        // 30行ごとにファイルに書き込む
        if (buffer_count == LINES_PER_WRITE) {
            for (int i = 0; i < buffer_count; ++i) {
                fputs(parsed_data_buffer[i], outfile);
                free(parsed_data_buffer[i]);
            }
            buffer_count = 0;
        }
    }

    // ループ終了後、バッファに残っているデータをすべて書き込む
    if (buffer_count > 0) {
        for (int i = 0; i < buffer_count; ++i) {
            fputs(parsed_data_buffer[i], outfile);
            free(parsed_data_buffer[i]);
        }
    }

    // ファイルを閉じる
    fclose(infile);
    fclose(outfile);

    fprintf(stdout, "Log analysis completed. Output written to '%s'\n", output_filepath);
    free(output_filepath);

    return 0;
}

// ヘルプメッセージの表示
void print_help(const char* program_name) {
    fprintf(stdout, "Usage: %s <input_file> [output_file]\n", program_name);
    fprintf(stdout, "       %s -h | ? | -?\n\n", program_name);
    fprintf(stdout, "This program parses a log file and extracts specific data into a CSV format.\n");
    fprintf(stdout, "If only <input_file> is provided, the output file will be named ");
    fprintf(stdout, "<input_file_basename>.csv in the same directory.\n");
    fprintf(stdout, "Example:\n");
    fprintf(stdout, "  %s input.log\n", program_name);
    fprintf(stdout, "  %s input.log output.csv\n", program_name);
}

// 出力ファイル名を生成するヘルパー関数
char* get_output_filename(const char* input_filename) {
    char* dot_pos = strrchr(input_filename, '.');
    size_t base_len = (dot_pos != NULL) ? (size_t)(dot_pos - input_filename) : strlen(input_filename);
    
    size_t output_len = base_len + strlen(".csv") + 1; 
    char* output_filename = (char*)malloc(output_len);
    if (output_filename == NULL) {
        return NULL;
    }

    strncpy(output_filename, input_filename, base_len);
    output_filename[base_len] = '\0';
    strcat(output_filename, ".csv");

    return output_filename;
}

// 文字列を大文字に変換するヘルパー関数
void to_upper(char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = str[i] - 32;
        }
    }
}