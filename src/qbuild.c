#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dir.h>
#include <io.h>
#include <sys\stat.h>

#define MAX_FILES 32
#define MAX_LINE  256
#define MAX_PATH  128
#define DOS_CMD_LIMIT 127

char tasm_path[MAX_PATH];
char qb45_path[MAX_PATH];

char tasm_exe[MAX_PATH];
char bc_exe[MAX_PATH];
char link_exe[MAX_PATH];
char bqlb_lib[MAX_PATH];
char bcom_lib[MAX_PATH];

FILE *logfp;

/* -------------------------------------------------- */

void fail(char *msg)
{
    printf("\nERROR: %s\n", msg);
    if (logfp) fprintf(logfp, "ERROR: %s\n", msg);
    exit(1);
}

int file_exists(char *p)
{
    return access(p, 0) == 0;
}

void ensure_dir(char *path)
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

void check_cmd_length(char *exe, char *args)
{
    int total = strlen(exe) + 1 + strlen(args);

    if (total >= DOS_CMD_LIMIT)
    {
        printf("\nERROR: Command line too long (%d chars).\n", total);
        printf("DOS limit is %d characters.\n", DOS_CMD_LIMIT);
        printf("Command was:\n%s %s\n\n", exe, args);

        if (logfp)
        {
            fprintf(logfp, "ERROR: Command too long (%d chars)\n", total);
            fprintf(logfp, "%s %s\n", exe, args);
        }

        exit(1);
    }
}

void join_path(char *out, char *a, char *b)
{
    strcpy(out, a);
    if (out[strlen(out)-1] != '\\')
        strcat(out, "\\");
    strcat(out, b);
}

void strip_ext(char *out, char *in)
{
    char *dot;
    strcpy(out, in);
    dot = strrchr(out, '.');
    if (dot) *dot = 0;
}

void trim(char *s)
{
    char *p;

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        memmove(s, s+1, strlen(s));

    p = s + strlen(s) - 1;

    while (p >= s && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        *p-- = 0;
}

void log_command(char *exe, char *arg)
{
    printf("\nCOMMAND:\n%s %s\n\n", exe, arg);

    if (logfp)
    {
        fprintf(logfp, "\nCOMMAND:\n%s %s\n\n", exe, arg);
        fflush(logfp);
    }
}

void write_rsp(char *path, char *content)
{
    FILE *fp = fopen(path, "w");
    if (!fp) fail("Cannot write response file.");

    fprintf(fp, "%s", content);
    fclose(fp);

    if (logfp)
    {
        fprintf(logfp, "\nRESPONSE FILE: %s\n", path);
        fprintf(logfp, "--------------------------\n");
        fprintf(logfp, "%s", content);
        fprintf(logfp, "--------------------------\n\n");
        fflush(logfp);
    }
}

void run_tool(char *exe, char *arg)
{
    char fullcmd[512];

    sprintf(fullcmd, "%s %s >> build\\build.log", exe, arg);

    printf("EXEC: %s\n", fullcmd);

    if (system(fullcmd) != 0)
        fail("Tool execution failed.");
}

/* -------------------------------------------------- */

void find_build_file(char *out)
{
    struct ffblk f;

    if (findfirst("*.bld", &f, 0) != 0)
        fail("No .bld file found.");

    strcpy(out, f.ff_name);

    if (findnext(&f) == 0)
        fail("Multiple .bld files found.");
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

    char objlist[512] = "";
    char exe_name[MAX_PATH];
    char map_name[MAX_PATH];
    char cmdline[256];

    printf("QBUILD starting...\n");

    clean_build();

    ensure_dir("build");
    ensure_dir("build\\obj");
    ensure_dir("build\\obj\\asm");
    ensure_dir("build\\obj\\qb");
    ensure_dir("build\\bin");
    ensure_dir("build\\lib");

    logfp = fopen("build\\build.log", "w");

    find_build_file(bld_file);
    parse_ini();

    /* Parse .bld */

    {
        FILE *fp;
        char line[MAX_LINE];
        int section = 0;

        fp = fopen(bld_file, "r");

        while (fgets(line, MAX_LINE, fp))
        {
            trim(line);
            if (strlen(line) == 0) continue;

            if (strcmp(line, "[name]") == 0) { section = 1; continue; }
            if (strcmp(line, "[qb]") == 0)   { section = 2; continue; }
            if (strcmp(line, "[asm]") == 0)  { section = 3; continue; }

            if (section == 1)
                strcpy(proj_name, line);
            else if (section == 2)
                strcpy(qb_files[qb_count++], line);
            else if (section == 3)
                strcpy(asm_files[asm_count++], line);
        }

        fclose(fp);
    }

    /* ----- Assemble ----- */

    for (i = 0; i < asm_count; i++)
    {
        char rsp[512];

        strip_ext(base, asm_files[i]);

        sprintf(obj, "build\\obj\\asm\\%s.obj", base);
        sprintf(lst, "build\\obj\\asm\\%s.lst", base);

        sprintf(cmdline, "/l %s,%s,%s", asm_files[i], obj, lst);
        log_command(tasm_exe, cmdline);

        sprintf(rsp, "%s\n", cmdline);
        write_rsp("build\\step.rsp", rsp);

        run_tool(tasm_exe, "@build\\step.rsp");

        if (!file_exists(obj))
            fail("ASM object file not created.");

        strcat(objlist, obj);
        if (i < asm_count-1 || qb_count > 0)
            strcat(objlist, " ");
    }

    /* ----- Compile BASIC ----- */

    for (i = 0; i < qb_count; i++)
    {
        strip_ext(base, qb_files[i]);

        sprintf(obj, "build\\obj\\qb\\%s.obj", base);
        sprintf(lst, "build\\obj\\qb\\%s.lst", base);

        sprintf(cmdline, "/o %s %s %s",
                qb_files[i],
                obj,
                lst);

        check_cmd_length(bc_exe, cmdline);
        log_command(bc_exe, cmdline);

        run_tool(bc_exe, cmdline);

        if (!file_exists(obj))
            fail("BASIC object file not created.");

        strcat(objlist, obj);
        if (i < qb_count-1)
            strcat(objlist, " ");
    }

    /* ----- Build QLB ----- */

    if (asm_count > 0)
    {
        char rsp[2048];
        char asmlist[1024] = "";

        for (i = 0; i < asm_count; i++)
        {
            strip_ext(base, asm_files[i]);
            sprintf(obj, "build\\obj\\asm\\%s.obj", base);

            strcat(asmlist, obj);
            if (i < asm_count-1)
                strcat(asmlist, " ");
        }

        sprintf(cmdline, "/Q %s, build\\lib\\asm.qlb,build\\lib\\asm.map,%s",
                asmlist, bqlb_lib);
        log_command(link_exe, cmdline);

        sprintf(rsp, "/Q %s,\nbuild\\lib\\asm.qlb,\nbuild\\lib\\asm.map,\n%s\n",
                asmlist, bqlb_lib);

        write_rsp("build\\step.rsp", rsp);
        run_tool(link_exe, "@build\\step.rsp");

        if (!file_exists("build\\lib\\asm.qlb"))
            fail("QLB file not created.");
    }

    /* ----- Link EXE ----- */

    {
        char rsp[2048];

        sprintf(exe_name, "build\\bin\\%s.exe", proj_name);
        sprintf(map_name, "build\\bin\\%s.map", proj_name);

        sprintf(cmdline, "%s,%s,%s,%s",
                objlist, exe_name, map_name, bcom_lib);
        log_command(link_exe, cmdline);

        sprintf(rsp, "%s,\n%s,\n%s,\n%s\n",
                objlist, exe_name, map_name, bcom_lib);

        write_rsp("build\\step.rsp", rsp);
        run_tool(link_exe, "@build\\step.rsp");

        if (!file_exists(exe_name))
            fail("EXE file not created.");
    }

    /* ----- Generate project.mak ----- */

    {
        FILE *mak = fopen("build\\project.mak", "w");
        if (!mak) fail("Cannot create project.mak");

        for (i = 0; i < qb_count; i++)
            fprintf(mak, "..\\%s\n", qb_files[i]);

        fclose(mak);

        if (!file_exists("build\\project.mak"))
            fail("project.mak not created.");
    }

    /* ----- Generate launch.bat (FULL PATH QB) ----- */

    {
        FILE *bat = fopen("launch.bat", "w");
        if (!bat) fail("Cannot create launch.bat");

        fprintf(bat, "@echo off\n");
        fprintf(bat, "\"%s\\qb\" /l build\\lib\\asm.qlb build\\project.mak\n", qb45_path);

        fclose(bat);
    }

    printf("\nBuild complete.\n");

    if (logfp) fclose(logfp);

    return 0;
}
