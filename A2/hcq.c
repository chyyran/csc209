#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "hcq.h"
#define INPUT_BUFFER_SIZE 256
#define COURSE_CODE_LEN 6

/**
 * ---- Begin Helper Functions ---
 */

/**
 * A malloc that panics and quits on on ENOMEM 
 */
void *panic_malloc(size_t size)
{
    void *ptr;
    if ((ptr = malloc(size)) == NULL)
    {
        perror("malloc");
        exit(1);
        return NULL;
    }
    else
    {
        return ptr;
    }
}

/**
 * Frees a heap-allocated student
 */
void free_student(Student *s)
{
    free(s->arrival_time);
    free(s->name);
    free(s);
}

int compare_student(Student *s1, Student *s2)
{
    return strcmp(s1->name, s2->name);
}

/**
 * Detaches a student from the queue
 * However, this does not remove the pointer to
 * the next student in the linked list in the overall queue.
 * 
 * If the student does not exist, returns NULL.
 */
Student *detach_student(Student **stu_list, Student *s)
{
    if (!s)
        return NULL;
    if (!find_student(*stu_list, s->name))
        return NULL;

    Student *curr = *stu_list;
    Student *prev = NULL;
    // Do the overall first
    while (curr)
    {
        if (!compare_student(curr, s))
        {
            if (prev)
            {
                prev->next_overall = curr->next_overall;
            }
            else
            {
                *stu_list = curr->next_overall;
            }
        }

        prev = curr;
        curr = curr->next_overall;
    }

    curr = s->course->head;
    prev = NULL;

    while (curr)
    {
        if (!compare_student(curr, s))
        {
            if (prev)
            {
                prev->next_course = curr->next_course;
            }
            else
            {
                s->course->head = curr->next_course;
            }
        }

        prev = curr;
        curr = curr->next_course;

        if (curr == NULL)
        {
            s->course->tail = prev;
        }
    }

    return s;
}


int count_students_waiting(Student *stu_list, char *course_code)
{
    Student *s = stu_list;
    int count = 0;
    while (s) 
    {
        if (!strncmp(s->course->code, course_code, strlen(course_code))) {
            count++;
        }
        // any student in the queue is still waiting...
        s = s->next_overall;
    }

    return count;
}


int count_students_being_helped(Ta *ta_list, char *course_code)
{
    Ta *t = ta_list;
    int count = 0;

    while (t)
    {
        Student *s = t->current_student;
        if (s && !strncmp(s->course->code, course_code, strlen(course_code))) {
            count++;
        }
        t = t->next;
    }
    return count;
}

#define malloc panic_malloc

/**
 * ---- End Helper Functions ---
 */

/*
 * Return a pointer to the struct student with name stu_name
 * or NULL if no student with this name exists in the stu_list
 */
Student *find_student(Student *stu_list, char *student_name)
{
    Student *next = stu_list;
    while (next)
    {
        if (!strncmp(next->name, student_name, strlen(student_name)))
        {
            return next;
        }
        next = next->next_overall;
    }
    return NULL;
}

/*   Return a pointer to the ta with name ta_name or NULL
 *   if no such TA exists in ta_list. 
 */
Ta *find_ta(Ta *ta_list, char *ta_name)
{
    Ta *next = ta_list;
    while (next)
    {
        if (!strncmp(next->name, ta_name, strlen(ta_name)))
        {
            return next;
        }
        next = next->next;
    }
    return NULL;
}

/*  Return a pointer to the course with this code in the course list
 *  or NULL if there is no course in the list with this code.
 */
Course *find_course(Course *courses, int num_courses, char *course_code)
{

    for (int i = 0; i < num_courses; i++)
    {
        if (!strncmp(courses[i].code, course_code, COURSE_CODE_LEN))
        {
            return &courses[i];
        }
    }
    return NULL;
}

/* Add a student to the queue with student_name and a question about course_code.
 * if a student with this name already has a question in the queue (for any
   course), return 1 and do not create the student.
 * If course_code does not exist in the list, return 2 and do not create
 * the student struct.
 * For the purposes of this assignment, don't check anything about the 
 * uniqueness of the name. 
 */
int add_student(Student **stu_list_ptr, char *student_name, char *course_code,
                Course *course_array, int num_courses)
{

    Course *course = find_course(course_array, num_courses, course_code);
    if (!course)
        return 2;
    if (find_student(*stu_list_ptr, student_name))
        return 1;

    Student s;
    s.course = course;
    s.name = malloc(sizeof(char) * strlen(student_name) + 1);
    memset(s.name, 0, sizeof(char) * strlen(student_name) + 1);
    strncpy(s.name, student_name, strlen(student_name));
    s.next_course = NULL;
    s.next_overall = NULL;
    s.arrival_time = malloc(sizeof(time_t));
    *s.arrival_time = time(0);

    Student *last_overall = *stu_list_ptr;
    Student *last_course = course->tail;

    // Navigate to last overall
    while (last_overall)
    {
        if (!strncmp(last_overall->name, student_name, strlen(student_name)))
            return 1;

        if (!last_overall->next_overall)
        {
            // last
            break;
        }

        last_overall = last_overall->next_overall;
    }

    Student *student_heap = malloc(sizeof(Student));
    memcpy(student_heap, &s, sizeof(Student));

    if (last_overall)
    {
        last_overall->next_overall = student_heap;
    }
    else
    {
        *stu_list_ptr = student_heap;
    }

    if (last_course)
    {
        last_course->next_course = student_heap;
        course->tail = student_heap;
    }
    else
    {
        course->head = student_heap;
        course->tail = student_heap;
    }
    return 0;
}

/* Student student_name has given up waiting and left the help centre
 * before being called by a Ta. Record the appropriate statistics, remove
 * the student from the queues and clean up any no-longer-needed memory.
 *
 * If there is no student by this name in the stu_list, return 1.
 */
int give_up_waiting(Student **stu_list_ptr, char *student_name)
{
    if (!find_student(*stu_list_ptr, student_name))
        return 1;

    Student *s = detach_student(stu_list_ptr, find_student(*stu_list_ptr, student_name));

    time_t now = time(0);
    time_t wait = *(s->arrival_time);
    s->course->bailed++;
    s->course->wait_time += (now - wait);

    free_student(s);
    return 0;
}

/* Create and prepend Ta with ta_name to the head of ta_list. 
 * For the purposes of this assignment, assume that ta_name is unique
 * to the help centre and don't check it.
 */
void add_ta(Ta **ta_list_ptr, char *ta_name)
{
    // first create the new Ta struct and populate
    Ta *new_ta = malloc(sizeof(Ta));
    if (new_ta == NULL)
    {
        perror("malloc for TA");
        exit(1);
    }
    new_ta->name = malloc(strlen(ta_name) + 1);
    if (new_ta->name == NULL)
    {
        perror("malloc for TA name");
        exit(1);
    }
    strcpy(new_ta->name, ta_name);
    new_ta->current_student = NULL;

    // insert into front of list
    new_ta->next = *ta_list_ptr;
    *ta_list_ptr = new_ta;
}

/* The TA ta is done with their current student. 
 * Calculate the stats (the times etc.) and then 
 * free the memory for the student. 
 * If the TA has no current student, do nothing.
 */
void release_current_student(Ta *ta)
{
    if (!ta->current_student)
        return;

    time_t help_start = *(ta->current_student->arrival_time);
    time_t total_helped = time(0) - help_start;

    ta->current_student->course->helped++;
    ta->current_student->course->help_time += total_helped;

    free_student(ta->current_student);
    ta->current_student = NULL;
}

/* Remove this Ta from the ta_list and free the associated memory with
 * both the Ta we are removing and the current student (if any).
 * Return 0 on success or 1 if this ta_name is not found in the list
 */
int remove_ta(Ta **ta_list_ptr, char *ta_name)
{
    Ta *head = *ta_list_ptr;
    if (head == NULL)
    {
        return 1;
    }
    else if (strcmp(head->name, ta_name) == 0)
    {
        // TA is at the head so special case
        *ta_list_ptr = head->next;
        release_current_student(head);
        // memory for the student has been freed. Now free memory for the TA.
        free(head->name);
        free(head);
        return 0;
    }
    while (head->next != NULL)
    {
        if (strcmp(head->next->name, ta_name) == 0)
        {
            Ta *ta_tofree = head->next;
            //  We have found the ta to remove, but before we do that
            //  we need to finish with the student and free the student.
            //  You need to complete this helper function
            release_current_student(ta_tofree);

            head->next = head->next->next;
            // memory for the student has been freed. Now free memory for the TA.
            free(ta_tofree->name);
            free(ta_tofree);
            return 0;
        }
        head = head->next;
    }
    // if we reach here, the ta_name was not in the list
    return 1;
}

/* TA ta_name is finished with the student they are currently helping (if any)
 * and are assigned to the next student in the full queue. 
 * If the queue is empty, then TA ta_name simply finishes with the student 
 * they are currently helping, records appropriate statistics, 
 * and sets current_student for this TA to NULL.
 * If ta_name is not in ta_list, return 1 and do nothing.
 */
int take_next_overall(char *ta_name, Ta *ta_list, Student **stu_list_ptr)
{
    Ta *ta;
    if (!(ta = find_ta(ta_list, ta_name)))
        return 1;

    Student *s = detach_student(stu_list_ptr, *stu_list_ptr);

    release_current_student(ta);

    if (s)
    {
        time_t now = time(0);
        time_t wait = *(s->arrival_time);
        s->course->wait_time += (now - wait);
    }

    ta->current_student = s;

    return 0;
}

/* TA ta_name is finished with the student they are currently helping (if any)
 * and are assigned to the next student in the course with this course_code. 
 * If no student is waiting for this course, then TA ta_name simply finishes 
 * with the student they are currently helping, records appropriate statistics,
 * and sets current_student for this TA to NULL.
 * If ta_name is not in ta_list, return 1 and do nothing.
 * If course is invalid return 2, but finish with any current student. 
 */
int take_next_course(char *ta_name, Ta *ta_list, Student **stu_list_ptr, char *course_code, Course *courses, int num_courses)
{

    Ta *ta;
    if (!(ta = find_ta(ta_list, ta_name)))
        return 1;

    release_current_student(ta);
    if (!find_course(courses, num_courses, course_code)) return 2;

    Student *s = *stu_list_ptr;

    while (s) {
        if (!strncmp(s->course->code, course_code, strlen(course_code))) break;
        s = s->next_overall;
    }
    
    s = detach_student(stu_list_ptr, s);


    if (s)
    {
        time_t now = time(0);
        time_t wait = *(s->arrival_time);
        s->course->wait_time += (now - wait);
    }

    ta->current_student = s;

    return 0;
}

/* For each course (in the same order as in the config file), print
 * the <course code>: <number of students waiting> "in queue\n" followed by
 * one line per student waiting with the format "\t%s\n" (tab name newline)
 * Uncomment and use the printf statements below. Only change the variable
 * names.
 */
void print_all_queues(Student *stu_list, Course *courses, int num_courses)
{

    for (int i = 0; i < num_courses; i++)
    {

        int count = 0;
        Student *s = courses[i].head;
        while (s)
        {
            count++;
            s = s->next_course;
        }

        printf("%s: %d in queue\n", courses[i].code, count);
        s = courses[i].head;
        while (s)
        {
            printf("\t%s\n", s->name);
            s = s->next_course;
        }
    }
}

/*
 * Print to stdout, a list of each TA, who they are serving at from what course
 * Uncomment and use the printf statements 
 */
void print_currently_serving(Ta *ta_list)
{
    if (!ta_list)
    {
        printf("No TAs are in the help centre.\n");
        return;
    }

    while (ta_list)
    {
        if (ta_list->current_student)
        {
            printf("TA: %s is serving %s from %s\n", ta_list->name, ta_list->current_student->name, ta_list->current_student->course->code);
        }
        else
        {
            printf("TA: %s has no student\n", ta_list->name);
        }

        ta_list = ta_list->next;
    }
}

/*  list all students in queue (for testing and debugging)
 *   maybe suggest it is useful for debugging but not included in marking? 
 */
void print_full_queue(Student *stu_list)
{
}

/* Prints statistics to stdout for course with this course_code
 * See example output from assignment handout for formatting.
 *
 */
int stats_by_course(Student *stu_list, char *course_code, Course *courses, int num_courses, Ta *ta_list)
{

    // TODO: students will complete these next pieces but not all of this
    //       function since we want to provide the formatting

    // You MUST not change the following statements or your code
    //  will fail the testing.

    Course *found = find_course(courses, num_courses, course_code);
    if (!found)
        return 1;

    int students_waiting = count_students_waiting(stu_list, course_code);
    int students_being_helped = count_students_being_helped(ta_list, course_code);

    printf("%s:%s \n", found->code, found->description);
    printf("\t%d: waiting\n", students_waiting);
    printf("\t%d: being helped currently\n", students_being_helped);
    printf("\t%d: already helped\n", found->helped);
    printf("\t%d: gave_up\n", found->bailed);
    printf("\t%f: total time waiting\n", found->wait_time);
    printf("\t%f: total time helping\n", found->help_time);

    return 0;
}

/* Dynamically allocate space for the array course list and populate it
 * according to information in the configuration file config_filename
 * Return the number of courses in the array.
 * If the configuration file can not be opened, call perror() and exit.
 */
int config_course_list(Course **courselist_ptr, char *config_filename)
{

    FILE *config = fopen(config_filename, "r");

    if (!config)
    {
        perror("fopen");
        exit(1);
    }

    char buf[INPUT_BUFFER_SIZE];

    fseek(config, 0, SEEK_SET);

    if (!fgets(buf, INPUT_BUFFER_SIZE, config))
    {
        perror("fgets");
        exit(1);
    }

    int count = strtol(buf, NULL, 10);

    Course courses[count];

    for (int i = 0; i < count; i++)
    {
        char code[COURSE_CODE_LEN + 1];
        char desc[INPUT_BUFFER_SIZE - 7];

        if (!fgets(buf, INPUT_BUFFER_SIZE, config))
        {
            break;
        }

        if (sscanf(buf, "%6s %[^\t\n]", code, desc) == 2)
        {
            Course *c = &courses[i];

            // Initialize Course
            c->bailed = 0;
            c->head = NULL;
            c->help_time = 0;
            c->tail = NULL;
            c->wait_time = 0;
            c->helped = 0;
            c->description = malloc(sizeof(char) * strlen(desc));

            // Copy buffers
            strcpy(c->code, code);
            strcpy(c->description, desc);
            // Flush buffers
            memset(code, 0, sizeof(code));
            memset(desc, 0, sizeof(desc));
        }
    }

    // Copy entire array to heap
    *courselist_ptr = malloc(sizeof(courses));
    memcpy(*courselist_ptr, &courses, sizeof(courses));
    return count;
}
