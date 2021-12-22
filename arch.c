#include "archiver.h"

const char* INFO_FILE_NAME = "info.txt";
const int I_START = 4;
const int ARC_FILE_NOT_OPENED = -1;
const int FILE_TO_ARC_NOT_OPENED = -2;

// Эта функция считает количество разрядов во входном числе.
int digs(double w)
{
	int yield = 0;
	while (w > 10) { yield++; w /= 10; }
	return yield + 1;
}

// Эта функция возвращает имя файла full_name
// без полного пути к нему
char* getBareName(char* full_name)
{
	char buffer[255];
	strcpy(buffer, full_name);
	char* tok = strtok(buffer, "/");
	int toks = 0;
	char token[255];

	while (tok)
	{
		if (strlen(tok) == 0) break;
		strcpy(token, tok);
		tok = strtok(NULL, "/");
		toks++;
	}

	if (toks == 0)
		return full_name;
	return token;
}

// Эта функция создаёт временный файл с именем INFO_FILE_NAME,
// в который записывает информацию об архивированных файлах, необходимую
// для будущей распаковки архива. Возвращает 0.
// Если не удалось открыть файл архивации: возвращает -2
int collectInfo(char** argv, int argc)
{
	int bytes_sizeInfo = 0;
	int fileCount = argc - I_START;

	// Таблица информации о всех файлах (размер в байтах, имя)
	char** bytes_file_info = (char**)malloc(sizeof(char*) * fileCount);
	
	int i_bfi = 0; // переменная хранит позицию в bytes_file_info
	// Для каждого архивируемого файла:
	for (int i_argv = I_START; i_argv < argc; ++i_argv)
	{

		FILE* file = fopen(argv[i_argv], "rb");
		if (!file)
		{
			return FILE_TO_ARC_NOT_OPENED;
		}

		// Узнаём размер файла
		fseek(file, 0, SEEK_END);
		int size = ftell(file);

		// Записываем размер в строку
		char* byte_size = (char*)malloc((digs(size) + 1) * sizeof(char));
		_itoa(size, byte_size, 10);

		fclose(file);

		// Прибавляем размер файла и размер имени файла к общему размеру информационного блока
		bytes_sizeInfo += digs(size);
		bytes_sizeInfo += strlen(argv[i_argv]);
		bytes_sizeInfo += 4;
		int countBytes = (digs(size) + 4 + strlen(argv[i_argv]));

		// Записываем всё в буффер
		char* bytes_current_file_info = (char*)malloc((countBytes + 1 )* sizeof(char));
		strcpy(bytes_current_file_info, byte_size);
		strcpy(bytes_current_file_info + digs(size), "||");

		char* name = getBareName(argv[i_argv]);

		strcpy(bytes_current_file_info + digs(size) + 2, name);
		strcpy(bytes_current_file_info + digs(size) + 2 + strlen(name), "||");

		// Добавляем информацию о файле к таблице информации о файлах
		bytes_file_info[i_bfi] = bytes_current_file_info;
		i_bfi++;

		free(byte_size);
	}

	// Добавляем размер разделителя между размером информационного блока и блока архивированных файлов
	bytes_sizeInfo += 2;

	// Записываем общий размер информации в строку
	char* bytes_size = (char*)malloc((digs(bytes_sizeInfo) + 1 ) * sizeof(char));
	_itoa(bytes_sizeInfo, bytes_size, 10);

	// Все заносим во временный файл info.txt
	FILE* info = fopen("info.txt", "wb");
	fputs(bytes_size, info);
	fputs("||", info);
	for (int i = 0; i < fileCount; ++i)
	{
		fputs(bytes_file_info[i], info);
	}

	// Освобождаем память, закрываем файл
	fclose(info);
	free(bytes_size);
	for (int i = 0; i < fileCount; ++i)
	{
		free(bytes_file_info[i]);
	}
	free(bytes_file_info);

	return 0;
}

int create(int argc, char** argv, const char* fname)
{
	char byte[1];  // единичный буфер для считывания одного байта

	// Создаём инфо-блок
	if (collectInfo(argv, argc) == FILE_TO_ARC_NOT_OPENED)
	{
		return FILE_TO_ARC_NOT_OPENED;
	}

	FILE* arc = fopen(fname, "wb");  // файл архив
	if (!arc)
	{
		return ARC_FILE_NOT_OPENED;
	}
	FILE* info = fopen(INFO_FILE_NAME, "rb");  // файл с инфо-блоком

	// Переписываем инфо-блок в архив
	while (!feof(info))
	{
		if (fread(byte, 1, 1, info) == 1) fwrite(byte, 1, 1, arc);
	}

	// Закрываем и удаляем инфо-блоковый файл
	fclose(info);
	remove(INFO_FILE_NAME);

	// Последовательная запись в архив архивируемых файлов побайтно :
	for (int i_argv = I_START; i_argv < argc; ++i_argv)
	{
		FILE* file = fopen(argv[i_argv], "rb");
		if (!file)
		{
			return FILE_TO_ARC_NOT_OPENED;
		}

		while (!feof(file))
		{
			if (fread(byte, 1, 1, file) == 1) fwrite(byte, 1, 1, arc);
		}

		fclose(file);
	}
	fclose(arc);

	return 0;
}

int extract(const char* fname)
{
	FILE* arc = fopen(fname, "rb");
	if (!arc)
	{
		return ARC_FILE_NOT_OPENED;
	}

	int max_int = 11;
	char info_block_size[11];   // размер информационного блока (с запасом под максимальное число int)
	
	char byte[1]; // буффер для считывания 1-го байта

	// Считываем размер инфо-блока
	int i = 0;
	while (!0)
	{
		fread(byte, 1, 1, arc);
		if (byte[0] != '|')
		{
			info_block_size[i] = byte[0];
			++i;
		}
		else break;
	}
	fread(byte, 1, 1, arc); // дочитываем вторую палку из разделителя ||
	info_block_size[i] = '\0';
	
	// Преобразуем размер инфо-блока из строки в число
	int _sz = atoi(info_block_size);

	// Вычитаем из него первый разделитель, который уже считали
	_sz-=2;
	char* info_block = (char*) malloc((_sz + 1) * sizeof(char));  // информационный блок, буффер 1
	char* info_block_2 = (char*)malloc((_sz + 1) * sizeof(char));  // информационный блок, буффер 2

	// Считываем инфо-блок в буффер
	for (int i = 0; i < _sz; ++i)
	{
		fread(info_block + i, sizeof(char), 1, arc);
	}
	info_block[_sz] = '\0';
	strcpy(info_block_2, info_block);

	// Парсим первый инфо-блок, узнаём количество файлов:
	char* tok = strtok(info_block, "||");
	int toks = 0;

	while (tok)
	{
		if (strlen(tok) == 0) break;
		tok = strtok(NULL, "||");
		toks++;
	}

	if (toks % 2 == 1) toks--;  // удаляем мусор
	int files = toks / 2;  // количество обнаруженных файлов в архиве

	// Парсим второй буффер, записывая размеры и имена файлов:
	char** tokens = (char*)malloc(sizeof(char*) * (toks + 1));
	tok = strtok(info_block_2, "||");
	toks = 0;

	while (tok)
	{
		if (strlen(tok) == 0) break;
		char* token = (char*)malloc(sizeof(char) * (strlen(tok) + 1));
		strcpy(token, tok);
		tokens[toks] = token;
		tok = strtok(NULL, "||");
		toks++;
	}

	// Процесс распаковки всех файлов :
	for (int i = 0; i < files; i++)
	{
		const char* size = tokens[i * 2];
		const char* name = tokens[i * 2 + 1];
		int _sz = atoi(size);

		// Имя распакованного файла
		char full_path[255];
		strcpy(full_path, "arc_");
		strcat(full_path, name);

		// Создаём файл с нужным именем и переписываем в него информацию из архива
		FILE* current_file = fopen(full_path, "wb");
		for (int r = 1; r <= _sz; r++)
		{
			if (fread(byte, 1, 1, arc) == 1) fwrite(byte, 1, 1, current_file);
		}
		fclose(current_file);
	}
	fclose(arc);

	// Освобождаем использованную память
	free(info_block);
	free(info_block_2);
	for (int i = 0; i < files * 2; ++i)
	{
		free(tokens[i]);
	}
	free(tokens);

	return 0;
}

char** getList(const char* fname, int* count_files)
{
	FILE* arc = fopen(fname, "rb");
	if (!arc)
	{
		return NULL;
	}

	int max_int = 11;
	char info_block_size[11];   // размер информационного блока (с запасом под максимальное число int)

	char byte[1]; // буффер для считывания 1-го байта

	// Считываем размер инфо-блока
	int i = 0;
	while (!0)
	{
		fread(byte, 1, 1, arc);
		if (byte[0] != '|')
		{
			info_block_size[i] = byte[0];
			++i;
		}
		else break;
	}
	fread(byte, 1, 1, arc); // дочитываем вторую палку из разделителя ||
	info_block_size[i] = '\0';

	// Преобразуем размер инфо-блока из строки в число
	int _sz = atoi(info_block_size);

	// Вычитаем из него первый разделитель, который уже считали
	_sz -= 2;
	char* info_block = (char*)malloc((_sz + 1) * sizeof(char));  // информационный блок, буффер 1
	char* info_block_2 = (char*)malloc((_sz + 1) * sizeof(char));  // информационный блок, буффер 2

	// Считываем инфо-блок в буффер
	for (int i = 0; i < _sz; ++i)
	{
		fread(info_block + i, sizeof(char), 1, arc);
	}
	info_block[_sz] = '\0';
	strcpy(info_block_2, info_block);

	// Парсим первый инфо-блок, узнаём количество файлов:
	char* tok = strtok(info_block, "||");
	int toks = 0;

	while (tok)
	{
		if (strlen(tok) == 0) break;
		tok = strtok(NULL, "||");
		toks++;
	}

	if (toks % 2 == 1) toks--;  // удаляем мусор
	int files = toks / 2;  // количество обнаруженных файлов в архиве

	// Парсим второй буффер, записывая размеры и имена файлов:
	char** tokens = (char*)malloc(sizeof(char*) * toks);
	tok = strtok(info_block_2, "||");
	toks = 0;

	while (tok)
	{
		if (strlen(tok) == 0) break;
		// Заносим в таблицу только имена, размеры пропускаем
		if (toks % 2 == 1)
		{
			char* token = (char*)malloc(sizeof(char) * (strlen(tok) + 1));
			strcpy(token, tok);
			tokens[(toks - 1) / 2] = token;
		}
		tok = strtok(NULL, "||");
		toks++;
	}

	// Освобождаем память
	free(info_block);
	free(info_block_2);

	*count_files = files;
	return tokens;
}

const char* COMMAND_FILE = "--file";
const char* COMMAND_CREATE = "--create";
const char* COMMAND_EXTRACT = "--extract";
const char* COMMAND_LIST = "--list";

int equalStr(char* str1, char* str2)
{
	if (strlen(str1) != strlen(str2))
		return -1;
	for (int i = 0; i < strlen(str1); ++i)
	{
		if (str1[i] != str2[i])
			return -1;
	}
	return 0;
}


int main(int argc, char** argv)
{
	char* com = argv[1];
	if (equalStr(argv[1], COMMAND_FILE) != 0)
	{
		printf("Error: expected '--file' in position 1.\n");
		return -1;
	}

	char* arc_name = argv[2];	/// expected .arc

	if (equalStr(argv[3], COMMAND_CREATE) == 0)
	{
		int result = create(argc, argv, arc_name);
		if (result == -1)
		{
			printf("Error: %s was not opened.\n", arc_name);
			return -1;
		}
		else if (result == -2)
		{
			printf("Error: some files from list were not opened.\n");
			return -2;
		}

		printf("File %s was successfully created.\n", arc_name);
	}
	else if (equalStr(argv[3], COMMAND_LIST) == 0)
	{
		int count_files = 0;
		char** list = getList(arc_name, &count_files);

		if (list == NULL)
		{
			printf("Error: %s was not opened.\n", arc_name);
			return -1;
		}

		printf("Files in %s:\n", arc_name);
		for (int i = 0; i < count_files; ++i)
		{
			printf("%i: %s\n", i + 1, list[i]);
		}
	}
	else if (equalStr(argv[3], COMMAND_EXTRACT) == 0)
	{
		if (extract(arc_name) == -1)
		{
			printf("Error: %s was not opened.\n", arc_name);
			return -1;
		}
		printf("Files with prefix 'arc_' were successfully extracted to the directory with archiver.'\n");
	}
	else
	{
		printf("Error: wrong command in position 3, expected '--create', '--extract', '--list'.\n");
		return -1;
	}

	return 0;
}
