#include <stdio.h>
#include <string.h>

#define LINE_LENGTH 100						/* Max line length in config file */

static void removeWhiteSpaces(char* string);

int main()
{
	FILE* inputFile;
	char singleLine[LINE_LENGTH];
	char lineDelimiter[2] = "-";
	char* singleWord;
	char* filename = "config.ini";
	char frequency[10] = "frequency";
	

	if ((inputFile = fopen(filename, "r")) == NULL)
	{
		printf("Error opening init file!\n");
		//return SC_ERROR;
		return -1;
	}

	while (fgets(singleLine, LINE_LENGTH, inputFile) != NULL)
	{
		singleWord = strtok(singleLine, lineDelimiter);

		puts(singleWord);

		if (strcmp(singleWord, frequency) == 0);
		{
			printf("ne smeta mu hm \n");
		}

		singleWord = strtok(NULL, lineDelimiter);

		removeWhiteSpaces(singleWord);

		puts(singleWord);
	}

	fclose(inputFile);

	return 0;
}

static void removeWhiteSpaces(char* word)
{
	int stringLen = strlen(word);
	int i = 0;
	int j = 0;
	int k = stringLen - 1;
	char* startString = word;
	char* returnString;

	while (startString[i] == 32)
	{
		i++;
	}

	while (startString[k] == 32)
	{
		k--;
	}

	for (j = 0; j < (k - i); j++)
	{
		word[j] = startString[j+i];
	}

	word[k-i] = '\0';
}

