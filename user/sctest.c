#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
    int id;
    char *nargv[4];
    nargv[0] = "sctest";
    nargv[1] = "x";
    nargv[2] = 0;
    int pfd[2];
    char buf[64];
    int i;
    int rv;
    int fd;
    struct stat stat_st;
    char *s;

    /* For two tests, we will fork and then exec sctest with arguments.
     * If argv[1] = "a" then do the exit()/wait() test.
     * If argv[1] = "b" then do the kill()/wait() test.
     */
    if (argc > 1) {
        /* We did an exec on sctest to test exec. */
        if (argv[1][0] == 'a') {
            /* exec()/wait() test */
            printf("In child program: pid = %d\n", getpid());
            exit(0);
        } else if (argv[1][0] == 'b') {
            /* kill()/waitt() test */
            printf("In child program: pid = %d\n", getpid());
            printf("In child waiting to be killed.\n");
            /* Loop forever until killed by parent. */
            while(1) {
                sleep(10);
            }
        }
    }

    /* uptime() test */
    rv = uptime();
    printf("uptime() = %d\n", rv);
    
    /* fork()/exec()/wait() test */
    id = fork();

    if (id == 0) {
        nargv[1][0] = 'a';
        printf("In child process: pid = %d\n", getpid());
        exec("sctest", nargv);
    }

    /* Parent */
    id = wait(&id);

    /* fork()/exec()/kill()/wait() test */
    id = fork();

    if (id == 0) {
        nargv[1][0] = 'b';
        printf("In child process: pid = %d\n", getpid());
        exec("sctest", nargv);
    }

    for (i = 0; i < 3; i++) {
        sleep(10);
    }

    /* Kill child and wait. */
    kill(id);
    id = wait(&id);
    printf("wait() = %d\n", id);

    /* Pipes test with dup() */
    pipe(pfd);

    write(pfd[1], "hello", 5);
    fd = dup(pfd[0]);
    read(fd, buf, 5);
    buf[5] = '\0';
    printf("pipe read buf = %s\n", buf);

    close(pfd[0]);
    close(pfd[1]);
    close(fd);

    /* Directories and Files test */
    mkdir("foo");

    fd = open("foo/test.txt", O_CREATE | O_RDWR);
    write(fd, "sctest\n", 7);
    close(fd);

    fd = open ("foo/test.txt", O_RDONLY);
    read(fd, buf, 7);
    buf[8] = '\0';
    printf(buf);
    close(fd);

    /* mknod() test */
    rv = mknod("foo/fakeconsole", 3, 27);
    printf("mknod() = %d\n", rv);

    /* unlink() test */
    rv = mknod("foo/fakeconsole2", 3, 27);
    printf("mknod() = %d\n", rv);
    
    unlink("foo/fakeconsole2");

    /* fstat() test */
    fd = open("foo", O_RDONLY);
    rv = fstat(fd, &stat_st);
    printf("type of \"foo\" is %d\n", stat_st.type);
    close(fd);

    /* chdir() test */
    chdir("foo");

    /* link() test */
    link("test.txt", "test2.txt");

    /* unlink() test */
    unlink("test.txt");

    /* chdir() test */
    chdir("..");

    /* sbrk() test */
    s = (char *) sbrk(4096);
    strcpy(s, "more memory");
    printf("data at <%x> = \"%s\"\n", s, s);
    
    exit(0);
}
