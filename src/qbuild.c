#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dir.h>
#include <io.h>
#include <sys\stat.h>

#define QBUILD_VERSION "1.1.0"
#define MAX_FILES 64
#define MAX_LINE 256
#define MAX_PATH 260

char tasm_path[MAX_PATH];
char qb45_path[MAX_PATH];

char tasm_exe[MAX_PATH];
char bc_exe[MAX_PATH];
char link_exe[MAX_PATH];
char bqlb_lib[MAX_PATH];
char bcom_lib[MAX_PATH];

FILE *logfp;

/* -------------------------------------------------- */

void fail(const char *msg)
{
    printf("\nERROR: %s\n", msg);
    if (logfp)
        fprintf(logfp, "ERROR: %s\n", msg);
    exit(1);
}

int file_exists(const char *p)
{
    return access(p, 0) == 0;
}

void ensure_dir(const char *path)
{
    mkdir(path);
}

void clean_build(void)
{
    printf("Cleaning build directory...\n");

    system("if exist build del build\\*.* > nul");
    system("if exist build\\obj del build\\obj\\*.* > nul");
    system("if exist build\\obj\\asm del build\\obj\\asm\\*.* > nul");
    system("if exist build\\obj\\qb del build\\obj\\qb\\*.* > nul");
    system("if exist build\\bin del build\\bin\\*.* > nul");
    system("if exist build\\lib del build\\lib\\*.* > nul");
}

void join_path(char *out, const char *a, const char *b)
{
    strcpy(out, a);
    if (out[strlen(out) - 1] != '\\')
        strcat(out, "\\");
    strcat(out, b);
}

void trim(char *s)
{
    char *p;

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        memmove(s, s + 1, strlen(s));

    p = s + strlen(s) - 1;

    while (p >= s && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        *p-- = 0;
}

void strip_ext(char *out, const char *in)
{
    char *dot;
    strcpy(out, in);
    dot = strrchr(out, '.');
    if (dot)
        *dot = 0;
}

void filename_only(char *out, const char *in)
{
    const char *p = strrchr(in, '\\');
    if (p)
        strcpy(out, p + 1);
    else
        strcpy(out, in);
}

void run_tool(const char *exe, const char *arg)
{
    char fullcmd[512];

    sprintf(fullcmd, "%s %s >> build\\build.log", exe, arg);
    printf("EXEC: %s\n", fullcmd);

    if (system(fullcmd) != 0)
        fail("Tool execution failed.");
}

/* -------------------------------------------------- */

int qb_severe_errors(const char *lstfile)
{
    FILE *fp;
    char line[256];
    int errors = 0;

    fp = fopen(lstfile, "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, "Severe") && strstr(line, "Error"))
            errors = atoi(line);
    }

    fclose(fp);
    return errors;
}

/* -------------------------------------------------- */

void parse_ini(void)
{
    FILE *fp;
    char line[MAX_LINE];

    if (!file_exists("c:\\qbuild\\qbuild.ini"))
        fail("Missing qbuild.ini");

    fp = fopen("c:\\qbuild\\qbuild.ini", "r");

    while (fgets(line, MAX_LINE, fp))
    {
        trim(line);

        if (strncmp(line, "TASM_PATH=", 10) == 0)
            strcpy(tasm_path, line + 10);

        if (strncmp(line, "QB45_PATH=", 10) == 0)
            strcpy(qb45_path, line + 10);
    }

    fclose(fp);

    join_path(tasm_exe, tasm_path, "TASM.EXE");
    join_path(bc_exe, qb45_path, "BC.EXE");
    join_path(link_exe, qb45_path, "LINK.EXE");
    join_path(bqlb_lib, qb45_path, "BQLB45.LIB");
    join_path(bcom_lib, qb45_path, "BCOM45.LIB");
}

/* -------------------------------------------------- */

int main(void)
{
    char bld_file[MAX_PATH];
    char proj_name[MAX_PATH];

    char qb_files[MAX_FILES][MAX_PATH];
    char asm_files[MAX_FILES][MAX_PATH];

    int qb_count = 0;
    int asm_count = 0;
    int i;

    char base[MAX_PATH];
    char obj[MAX_PATH];
    char lst[MAX_PATH];

    clean_build();

    ensure_dir("build");
    ensure_dir("build\\obj");
    ensure_dir("build\\obj\\asm");
    ensure_dir("build\\obj\\qb");
    ensure_dir("build\\bin");
    ensure_dir("build\\lib");

    logfp = fopen("build\\build.log", "w");

    /* Find .bld */
    {
        struct ffblk f;
        if (findfirst("*.bld", &f, 0) != 0)
            fail("No .bld file found.");
        strcpy(bld_file, f.ff_name);
    }

    parse_ini();

    /* Parse .bld */
    {
        FILE *fp;
        char line[MAX_LINE];
        int section = 0;

        fp = fopen(bld_file, "r");
        if (!fp)
            fail("Cannot open .bld file.");

        while (fgets(line, MAX_LINE, fp))
        {
            trim(line);
            if (!strlen(line))
                continue;

            if (!strcmp(line, "[name]"))
            {
                section = 1;
                continue;
            }
            if (!strcmp(line, "[qb]"))
            {
                section = 2;
                continue;
            }
            if (!strcmp(line, "[asm]"))
            {
                section = 3;
                continue;
            }

            if (section == 1)
                strcpy(proj_name, line);
            if (section == 2)
                strcpy(qb_files[qb_count++], line);
            if (section == 3)
                strcpy(asm_files[asm_count++], line);
        }

        fclose(fp);
    }

    /* ----- Assemble ----- */
    for (i = 0; i < asm_count; i++)
    {
        FILE *rsp;

        filename_only(base, asm_files[i]);
        strip_ext(base, base);

        sprintf(obj, "build\\obj\\asm\\%s.obj", base);
        sprintf(lst, "build\\obj\\asm\\%s.lst", base);

        if (file_exists(obj))
            remove(obj);
        if (file_exists(lst))
            remove(lst);

        rsp = fopen("build\\step.rsp", "w");
        fprintf(rsp, "/l %s,%s,%s\n", asm_files[i], obj, lst);
        fclose(rsp);

        run_tool(tasm_exe, "@build\\step.rsp");

        if (!file_exists(obj))
            fail("ASM object file not created.");
    }

    /* ----- Build QLB ----- */
    if (asm_count > 0)
    {
        FILE *rsp;

        rsp = fopen("build\\step.rsp", "w");
        if (!rsp)
            fail("Cannot create QLB rsp.");

        /* /Q flag must appear BEFORE object list */
        fprintf(rsp, "/Q ");

        /* All ASM objects using LINK response continuation */
        for (i = 0; i < asm_count; i++)
        {
            filename_only(base, asm_files[i]);
            strip_ext(base, base);

            if (i < asm_count - 1)
                fprintf(rsp, "build\\obj\\asm\\%s.obj+\n", base);
            else
                fprintf(rsp, "build\\obj\\asm\\%s.obj", base);
        }

        /* End object list */
        fprintf(rsp, ",\n");

        /* Output QLB */
        fprintf(rsp, "build\\lib\\asm.qlb,\n");
        fprintf(rsp, "build\\lib\\asm.map,\n");
        fprintf(rsp, "%s\n", bqlb_lib);

        fclose(rsp);

        run_tool(link_exe, "@build\\step.rsp");

        if (!file_exists("build\\lib\\asm.qlb"))
            fail("QLB file not created.");
    }

    /* ----- Compile BASIC ----- */
    for (i = 0; i < qb_count; i++)
    {
        char cmd[512];
        int severe;

        filename_only(base, qb_files[i]);
        strip_ext(base, base);

        sprintf(obj, "build\\obj\\qb\\%s.obj", base);
        sprintf(lst, "build\\obj\\qb\\%s.lst", base);

        if (file_exists(obj))
            remove(obj);
        if (file_exists(lst))
            remove(lst);

        sprintf(cmd, "/o %s %s %s", qb_files[i], obj, lst);

        run_tool(bc_exe, cmd);

        if (!file_exists(lst))
            fail("QB listing file not created.");

        severe = qb_severe_errors(lst);

        if (severe < 0)
            fail("Could not read QB listing.");

        if (severe > 0)
            fail("QB compile failed.");

        if (!file_exists(obj))
            fail("QB object file not created.");
    }

    /* ----- Link EXE ----- */
    {
        FILE *rsp;
        char exe_name[MAX_PATH];
        char map_name[MAX_PATH];

        sprintf(exe_name, "build\\bin\\%s.exe", proj_name);
        sprintf(map_name, "build\\bin\\%s.map", proj_name);

        rsp = fopen("build\\step.rsp", "w");
        if (!rsp)
            fail("Cannot create LINK rsp file.");

        /* Write object list using LINK response continuation */
        for (i = 0; i < asm_count; i++)
        {
            filename_only(base, asm_files[i]);
            strip_ext(base, base);

            /* If there are more objects after this one, use +\n continuation */
            if (i < asm_count - 1 || qb_count > 0)
                fprintf(rsp, "build\\obj\\asm\\%s.obj+\n", base);
            else
                fprintf(rsp, "build\\obj\\asm\\%s.obj", base);
        }

        for (i = 0; i < qb_count; i++)
        {
            filename_only(base, qb_files[i]);
            strip_ext(base, base);

            if (i < qb_count - 1)
                fprintf(rsp, "build\\obj\\qb\\%s.obj+\n", base);
            else
                fprintf(rsp, "build\\obj\\qb\\%s.obj", base);
        }

        for (i = 0; i < qb_count; i++)
        {
            filename_only(base, qb_files[i]);
            strip_ext(base, base);
            fprintf(rsp, "build\\obj\\qb\\%s.obj ", base);
        }

        /* End object list */
        fprintf(rsp, ",\n");

        /* Now next prompts */
        fprintf(rsp, "%s,\n", exe_name);
        fprintf(rsp, "%s,\n", map_name);
        fprintf(rsp, "%s\n", bcom_lib);

        fclose(rsp);

        run_tool(link_exe, "@build\\step.rsp");

        if (!file_exists(exe_name))
            fail("EXE file not created.");
    }

    /* ----- Generate project.mak (used by launch.bat) ----- */
    {
        FILE *mak = fopen("build\\project.mak", "w");
        if (!mak)
            fail("Cannot create project.mak");

        /* .bld contains correct relative paths for [qb] files */
        for (i = 0; i < qb_count; i++)
            fprintf(mak, "..\\%s\n", qb_files[i]);

        fclose(mak);

        if (!file_exists("build\\project.mak"))
            fail("project.mak not created.");
    }

    /* ----- Generate launch.bat ----- */
    {
        FILE *bat = fopen("launch.bat", "w");
        if (!bat)
            fail("Cannot create launch.bat");

        fprintf(bat, "@echo off\n");

        if (file_exists("build\\lib\\asm.qlb"))
            fprintf(bat, "\"%s\\qb\" /l build\\lib\\asm.qlb build\\project.mak\n", qb45_path);
        else
            fprintf(bat, "\"%s\\qb\" build\\project.mak\n", qb45_path);

        fclose(bat);
    }

    printf("\nBuild complete.\n");

    if (logfp)
        fclose(logfp);

    return 0;
}
