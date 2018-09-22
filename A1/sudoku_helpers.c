#include <stdio.h>

/* Each of the n elements of array elements, is the address of an
 * array of n integers.
 * Return 0 if every integer is between 1 and n^2 and all
 * n^2 integers are unique, otherwise return 1.
 */
int check_group(int **elements, int n)
{
    // TODO: replace this return statement with a real function body
    int numbers[n * n];
    int count = 0;
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            if (elements[i][j] < 1 || elements[i][j] > n * n)
                return 1;
            numbers[count] = elements[i][j];
            count++;
        }
    }

    for (int i = 0; i < n * n; ++i)
    {
        for (int j = 0; j < n * n; ++j)
        {
            if (i != j && numbers[i] == numbers[j])
            {
                return 1;
            }
        }
    }
    return 0;
}

/* puzzle is a 9x9 sudoku, represented as a 1D array of 9 pointers
 * each of which points to a 1D array of 9 integers.
 * Return 0 if puzzle is a valid sudoku and 1 otherwise. You must
 * only use check_group to determine this by calling it on
 * each row, each column and each of the 9 inner 3x3 squares
 */
int check_regular_sudoku(int **puzzle)
{

    // Check rows
    int group1[3];
    int group2[3];
    int group3[3];
    int *group[3] = {group1, group2, group3};

    // Check Rows
    for (int i = 0; i < 9; i++)
    {
        group1[0] = puzzle[i][0];
        group1[1] = puzzle[i][1];
        group1[2] = puzzle[i][2];

        group2[0] = puzzle[i][3];
        group2[1] = puzzle[i][4];
        group2[2] = puzzle[i][5];

        group3[0] = puzzle[i][6];
        group3[1] = puzzle[i][7];
        group3[2] = puzzle[i][8];
        if (check_group(group, 3))
            return 1;
    }

    // Check cols
    for (int i = 0; i < 9; i++)
    {
        group1[0] = puzzle[0][i];
        group1[1] = puzzle[1][i];
        group1[2] = puzzle[2][i];

        group2[0] = puzzle[3][i];
        group2[1] = puzzle[4][i];
        group2[2] = puzzle[5][i];

        group3[0] = puzzle[6][i];
        group3[1] = puzzle[7][i];
        group3[2] = puzzle[8][i];
        if (check_group(group, 3))
            return 1;
    }

    // Check blocks
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            group1[0] = puzzle[0 + 3 * i][0 + 3 * j];
            group1[1] = puzzle[0 + 3 * i][1 + 3 * j];
            group1[2] = puzzle[0 + 3 * i][2 + 3 * j];

            group2[0] = puzzle[1 + 3 * i][0 + 3 * j];
            group2[1] = puzzle[1 + 3 * i][1 + 3 * j];
            group2[2] = puzzle[1 + 3 * i][2 + 3 * j];

            group3[0] = puzzle[2 + 3 * i][0 + 3 * j];
            group3[1] = puzzle[2 + 3 * i][1 + 3 * j];
            group3[2] = puzzle[2 + 3 * i][2 + 3 * j];
            if (check_group(group, 3))
                return 1;
        }
    }

    return 0;
}
