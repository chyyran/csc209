#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hcq.h"
#include "dynstr.h"
#include "panic.h"
#define OUT_BUF_SIZE 1024

/*
 * Return a pointer to the struct student with name stu_name
 * or NULL if no student with this name exists in the stu_list
 */
Student *find_student(Student *stu_list, const char *student_name)
{
    while (stu_list != NULL)
    {
        if (strcmp(stu_list->name, student_name) == 0)
        {
            return stu_list;
        }
        stu_list = stu_list->next_overall;
    }
    return NULL;
}

/*   Return a pointer to the ta with name ta_name or NULL
 *   if no such TA exists in ta_list. 
 */
Ta *find_ta(Ta *ta_list, const char *ta_name)
{
    Ta *current = ta_list;
    while (current != NULL && (strcmp(current->name, ta_name) != 0))
    {
        current = current->next;
    }
    return current;
}

/*  Return a pointer to the course with this code in the course list
 *  or NULL if there is no course in the list with this code.
 */
Course *find_course(Course *courses, int num_courses, char *course_code)
{
    for (int i = 0; i < num_courses; i++)
    {
        if (strcmp(courses[i].code, course_code) == 0)
        {
            return &courses[i];
        }
    }
    return NULL;
}

/* Add a student to the queue with student_name and a question about course_code.
 * if a student with this name already has a question in the queue (for any
 *  course), return 1 and do not create the student.
 * If course_code does not exist in the list, return 2 and do not create
 * the student struct.
 */
int add_student(Student **stu_list_ptr, const char *student_name, char *course_code,
                Course *course_array, int num_courses, Client *c)
{

    // check if this student is already in the queue and if so, don't add
    Student *old_student = find_student(*stu_list_ptr, student_name);
    if (old_student != NULL)
    {
        return 1;
    }

    // first create the new student struct and set the name
    Student *new_student = pamalloc(sizeof(Student));
    new_student->name = pamalloc(strlen(student_name) + 1);
    new_student->client = c;
    strcpy(new_student->name, student_name);

    // find the course in the course list and handle invalid course code errors
    new_student->course = find_course(course_array, num_courses, course_code);
    if (new_student->course == NULL)
    {
        free(new_student->name);
        free(new_student);
        return 2;
    }

    new_student->next_overall = NULL;

    // find the end of the current list to put this student at the end
    if (*stu_list_ptr == NULL)
    {
        // there is currently no student in the list at all, so special case
        *stu_list_ptr = new_student;
    }
    else
    {
        // we have at least one student in the list so find last student
        Student *last = *stu_list_ptr;
        while (last->next_overall != NULL)
        {
            last = last->next_overall;
        }
        // at this point last is the last real student in the overall list
        last->next_overall = new_student;
    }

    return 0;
}

/*
 * Helper method 
 */
void route_around_overall(Student **stu_list_ptr, Student *thisstudent)
{
    // find the previous student overall -- route around this removing one
    Student *current = *stu_list_ptr;
    if (current == thisstudent)
    {
        // this student is first in list
        *stu_list_ptr = thisstudent->next_overall;
    }
    else
    {
        while (current->next_overall != thisstudent)
        {
            current = current->next_overall;
        }
        // now current points to previous student
        current->next_overall = thisstudent->next_overall;
    }
}

/* Student student_name has given up waiting and left the help centre
 * before being called by a TA. Record the appropriate statistics, remove
 * the student from the queues and clean up any no-longer-needed memory.
 *
 * If there is no student by this name in the stu_list, return 1.
 */
int give_up_waiting(Student **stu_list_ptr, char *student_name)
{
    // find the student based on name from the student list
    Student *thisstudent = find_student(*stu_list_ptr, student_name);
    if (thisstudent == NULL)
    {
        return 1;
    }
    route_around_overall(stu_list_ptr, thisstudent);

    // free memory
    free(thisstudent->name);
    free(thisstudent);
    return 0;
}

/* Create and prepend TA with ta_name to the head of ta_list. 
 * For the purposes of this assignment, assume that ta_name is unique
 * to the help centre and don't check it.
 */
void add_ta(Ta **ta_list_ptr, const char *ta_name, Client *client)
{
    // first create the new TA struct and populate
    Ta *new_ta = pamalloc(sizeof(Ta));
    new_ta->name = pamalloc(strlen(ta_name) + 1);
    strcpy(new_ta->name, ta_name);
    new_ta->current_student = NULL;
    new_ta->client = client;
    // insert into front of list
    new_ta->next = *ta_list_ptr;
    *ta_list_ptr = new_ta;
}

/* remove this Ta from the ta_list and free the associated memory
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
        free(head);
        return 0;
    }
    while (head->next != NULL)
    {
        if (strcmp(head->next->name, ta_name) == 0)
        {
            Ta *tofree = head->next;
            head->next = head->next->next;
            free(tofree);
            return 0;
        }
    }
    // if we reach here, the ta_name was not in the list
    return 1;
}

/* Take student to_serve out of the stu_list and assign them as the currently
 *   being served sudent for TA with name ta_name. 
 * If there is another student currently being served then this finishes 
 * this student and records stats appropriately.
 * If to_serve is NULL (i.e. the method is called with no student to take), then
 * it records the completion of the existing student (if any) and frees 
 * If ta_name is not in ta_list, return 1 and take no further action.
 */
int take_student(Ta *ta, Student **stu_list_ptr, Student *to_serve)
{

    // find student being served if any and release
    Student *servee = ta->current_student;
    if (servee != NULL)
    {
        free(servee->name);
        free(servee);
    }
    //  set TA to point to the new (to-be-served) student
    ta->current_student = to_serve;
    //  if there is a new servee then remove them from the waiting queue
    //  but don't free the memory since they have just moved to the being
    //  served data structure
    if (to_serve != NULL)
    {
        route_around_overall(stu_list_ptr, to_serve);
    }
    return 0;
}

/* TA ta_name is finished with the student they are currently helping (if any)
 * and are assigned to the next student in the full queue. 
 * If the queue is empty, then TA ta_name simply finishes with the student 
 * they are currently helping, records appropriate statistics, 
 * and sets current_student for this TA to NULL.
 * If ta_name is not in ta_list, return 1 and do nothing.
 */
int next_overall(const char *ta_name, Ta **ta_list_ptr, Student **stu_list_ptr)
{
    //  find next student to serve  -- this is just the head of stu_list
    Student *to_serve_next = *stu_list_ptr;
    // find ta by name in ta_list
    Ta *ta = find_ta(*ta_list_ptr, ta_name);
    if (ta == NULL)
    {
        return 1;
    }
    return take_student(ta, stu_list_ptr, to_serve_next);
}

// print a message about which TAs are serving which students
char *print_currently_serving(Ta *ta_list)
{

    if (ta_list == NULL)
    {
        DynamicString *ds = ds_from_cstr("No TAs are currently working.\n");
        //printf("No TAs are currently working.\n");
        return ds_into_raw(ds);
    }

    char *buf;
    DynamicString* ds = ds_new(0);
    while (ta_list != NULL)
    {
        if (ta_list->current_student != NULL)
        {
            paasprintf(&buf, "TA: %s is serving %s.\n",
                   ta_list->name,
                   ta_list->current_student->name);
            ds_append(ds, buf);
        }
        else
        {
            paasprintf(&buf, "TA: %s has no student\r\n", ta_list->name);
            ds_append(ds, buf);
        }
        ta_list = ta_list->next;
    }

    return ds_into_raw(ds);
}

/*  Print a list of all the students in the queue. This does not
 *    print students currently being served. 
 */
char *print_full_queue(Student *stu_list)
{

    DynamicString *ds = ds_from_cstr("Full Queue\n");
    char *buf;
    while (stu_list != NULL)
    {
        paasprintf(&buf, "Student %s:%s\n", stu_list->name, stu_list->course->code);
        ds_append(ds, buf);
        free(buf);
        stu_list = stu_list->next_overall;
    }
    return ds_into_raw(ds);
}

/* Dynamically allocate space for the array course list and populate it
 * according to information in the configuration file config_filename
 * Return the number of courses in the array.
 * If the configuration file can not be opened, call perror() and exit.
*/

int config_course_list(Course **courselist_ptr, char *config_filename)
{
    /* for this stripped-down implementation, we are just ignoring
       the config_filename 
    */

    *courselist_ptr = malloc(sizeof(Course) * 3);
    if (*courselist_ptr == NULL)
    {
        perror("Malloc for course list\n");
        exit(1);
    }
    strcpy((*courselist_ptr)[0].code, "CSC108");
    strcpy((*courselist_ptr)[1].code, "CSC148");
    strcpy((*courselist_ptr)[2].code, "CSC209");
    return 3;
}
