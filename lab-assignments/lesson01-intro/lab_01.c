#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
//#define DEBUG




// Function to read number from input 
int getNumber(char **srcPtr)
{
	char buff[20] = "\0";
	char *destPtr = buff;
	int buffNumber = 0;
	while (**srcPtr != '\n')
	{
		// Skipping all whitespaces
		if (**srcPtr == ' ')
		{
			(*srcPtr)++;
			continue;
		}
		//If we getting a number, copying it to buffer string
		if (**srcPtr >= '0' && **srcPtr <= '9')
		{
			*(destPtr++) = **srcPtr;
			(*srcPtr)++;
		}
		else
		{
#ifdef DEBUG
			printf("Buff: %s\n", buff);
#endif // DEBUG
		//Converting out number from strig to int
			buffNumber = atoi(buff);
			break;
		}
	}
	if (**srcPtr == '\n')
	{
#ifdef DEBUG
		printf("Buff: %s\n", buff);
#endif // DEBUG
		buffNumber = atoi(buff);
	}
	return buffNumber;
}

// to reverse string
char *strrev(char *str)
{
      char *p1, *p2;

      if (! str || ! *str)
            return str;
      for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
      {
            *p1 ^= *p2;
            *p2 ^= *p1;
            *p1 ^= *p2;
      }
      return str;
}

//Same as getting number but going from right to left
int getNumberReversed(char **srcPtr)
{
	char buff[20] = "\0";
	char *destPtr = buff;
	int buffNumber = 0;
	--srcPtr;
	while (**srcPtr >= '0' && **srcPtr <= '9')
	{
		*(destPtr++) = **srcPtr;
		(*srcPtr)--;
	}
	strrev(buff);
#ifdef DEBUG
	printf("Buff: %s\n", buff);
#endif // DEBUG
	buffNumber = atoi(buff);
	return buffNumber;
}

//removing all  multiplyings
void removeMultiply(char *inputStr)
{
	char newInputStr[100] = "";
	char *srcPtr = inputStr;
	char multiply = '*';

	char *inputPointer;
	// Looking for mutiply symbol in our input
	inputPointer = strchr(inputStr, multiply);
	--inputPointer;
	// Moving pointer to the beginnig of 1st multiplier
	while (*inputPointer >= '0' && *inputPointer <= '9')
		--inputPointer;
	++inputPointer;
	// Saving pointer to where our multiplications starts
	char *inputPointerForCopy = inputPointer;

	// Getting first multiplier
	int num1 = getNumber(&inputPointer);

	size_t i = 0;
	// Copying input str to new str till our multiplication
	for (; srcPtr != inputPointerForCopy; i++, srcPtr++)
	{
		newInputStr[i] = *srcPtr;
	}
	// Going to the multiplication symbol to get 2nd multiplier
	inputPointer = strchr(inputStr, multiply);

	++inputPointer;
	int num2 = getNumber(&inputPointer);
	char chMult[10] = "\0";
	// Count  and  convert back to char
	sprintf(chMult, "%d", num1 * num2);
	// Inserting our multiplication result to new string
	for (size_t j = 0; chMult[j] != '\0'; ++i, ++j)
	{
		newInputStr[i] = chMult[j];
	}
	// Inserting the rest of string
	while (*inputPointer)
	{
		newInputStr[i] = *inputPointer;
		++inputPointer;
		++i;
	}
	// Assign new string to out old one
	for (size_t i = 0; inputStr[i] != '\n'; i++)
	{
		inputStr[i] = '\0';
	}
	for (size_t i = 0; newInputStr[i] != '\0'; i++)
	{
		inputStr[i] = newInputStr[i];
	}
}
//  All the same as for multiplication
void removeDivision(char *inputStr)
{
	char newInputStr[100] = "";
	char *srcPtr = inputStr;
	char division = '/';

	char *inputPointer;
	inputPointer = strchr(inputStr, division);
	--inputPointer;
	while (*inputPointer >= '0' && *inputPointer <= '9')
		--inputPointer;
	++inputPointer;
	char *inputPointerForCopy = inputPointer;
	int num1 = getNumber(&inputPointer);

	size_t i = 0;
	for (; srcPtr != inputPointerForCopy; i++, srcPtr++)
	{
		newInputStr[i] = *srcPtr;
	}

	inputPointer = strchr(inputStr, division);

	++inputPointer;
	int num2 = getNumber(&inputPointer);
	char chMult[10] = "\0";
	sprintf(chMult, "%d", num1 / num2);

	for (size_t j = 0; chMult[j] != '\0'; ++i, ++j)
	{
		newInputStr[i] = chMult[j];
	}

	while (*inputPointer)
	{
		newInputStr[i] = *inputPointer;
		++inputPointer;
		++i;
	}
	for (size_t i = 0; inputStr[i] != '\n'; i++)
	{
		inputStr[i] = '\0';
	}
	for (size_t i = 0; newInputStr[i] != '\0'; i++)
	{
		inputStr[i] = newInputStr[i];
	}
}

int main(int argc, char *argv[])
{
	while (1)
	{

		char inputStr[500];
		printf("Enter the expression:\n");
		fgets(inputStr, sizeof(inputStr), stdin);

		while (strchr(inputStr, '*'))
		{
			removeMultiply(&inputStr);
#ifdef DEBUG
			printf("%s\n", inputStr);

#endif // DEBUG
		}
		while (strchr(inputStr, '/'))
		{
			removeDivision(&inputStr);
#ifdef DEBUG

			printf("%s\n", inputStr);
#endif // DEBUG
		}

		char *srcPtr = inputStr;
		int sum = getNumber(&srcPtr);

		while (*srcPtr != '\n')
		{
			switch (*srcPtr)
			{
			case ('+'):
				srcPtr++;
				sum += getNumber(&srcPtr);
				break;
			case ('-'):
				srcPtr++;
				sum -= getNumber(&srcPtr);
				break;
			default:
				break;
			}
		}

		printf("%d\n", sum);
	}

	return 0;
}
