#ifndef HCQ_H
#define HCQ_H

#include "client.h"

/* Students are kept in order by time with newest 
   students at the end of the lists. */
struct student{
    char *name;
    struct course *course;
    struct student *next_overall;
    Client *client;
};

/* Tas are kept in reverse order of their time of addition. Newest
   Tas are kept at the head of the list. */ 
struct ta {
    char *name;
    struct student *current_student;
    struct ta *next;
    Client *client;
};

struct course{
    char code[7];
};


typedef struct student Student;
typedef struct course Course;
typedef struct ta Ta;

// helper functions not directly related to only one command in the API
Student *find_student(Student *stu_list, const char *student_name);
Ta *find_ta(Ta *ta_list, const char *ta_name);

// functions provided as the API to a help-centre queue
int add_student(Student **stu_list_ptr, const char *student_name, char *course_num,
    Course *courses, int num_courses, Client *c);
int give_up_waiting(Student **stu_list_ptr, char *student_name);

void add_ta(Ta **ta_list_ptr, const char *ta_name, Client *c);
int remove_ta(Ta **ta_list_ptr, Student **stu_list_ptr, char *ta_name);
int take_student(Ta *ta, Student **stu_list_ptr, Student *to_serve);

//  if student is currently being served then this finishes this student
//    if there is no-one else waiting then the currently being served gets
//    set to null 
int next_overall(const char *ta_name, Ta **ta_list_ptr, Student **stu_list_ptr);

// list currently being served by current TAs
char *print_currently_serving(Ta *ta_list);

// list all students in queue 
char *print_full_queue(Student *stu_list);

#endif
