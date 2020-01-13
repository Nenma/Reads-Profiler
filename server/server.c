#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>

#define PORT 2728
#define BOOKS_IN_COMMON 3

int err;

struct Client{
    int connfd;
    int tid;
    char username[100];
    struct sockaddr_in addr;
};


int callbackRegister(void *NotUsed, int argc, char **argv, char **azColName) {
    char* buffer = (char*) NotUsed;

    strcat(buffer, argv[0]);
    
    return 0;
}

int authenticationHandler(struct Client* cp) {
    //initializing database
    sqlite3 *db;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    //get whether it is login or register
    int option;
    read(cp->connfd, &option, sizeof(option));

    char username[100], password[100];
    if (option == 1) {
        int loggedIn = 0;
        sqlite3_stmt *res;
        do {
            int alreadyLoggedIn = 1;
            do {
                //get the username
                read(cp->connfd, username, 100);

                char sql[200] = {0};
                sprintf(sql, "SELECT Id FROM LoggedIn WHERE Username = '%s';", username);

                char buffer[20] = {0};
                rc = sqlite3_exec(db, sql, callbackRegister, buffer, &err_msg);
                if (rc != SQLITE_OK) {
                    fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, err_msg);
                    sqlite3_free(err_msg);
                    sqlite3_close(db);
                    return -1;
                }

                if (strlen(buffer) == 0) {
                    alreadyLoggedIn = 0;
                }

                write(cp->connfd, &alreadyLoggedIn, sizeof(alreadyLoggedIn));
            } while (alreadyLoggedIn);
            //get the password
            read(cp->connfd, password, 100);

            rc = sqlite3_prepare_v2(db, "SELECT Password FROM Users WHERE Username = @usrn;", -1, &res, 0);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
                sqlite3_close(db);
                return -1;
            } else {
                int unx = sqlite3_bind_parameter_index(res, "@usrn");
                sqlite3_bind_text(res, unx, username, -1, 0);
            }

            rc = sqlite3_step(res);
            if (rc == SQLITE_ROW) {
                if(strcmp(sqlite3_column_text(res, 0), password) == 0) {
                    printf("[%d]Client %d was successfully logged in as %s!\n", cp->connfd, cp->connfd, username);
                    loggedIn = 1;

                    char sql[200] = {0};
                    sprintf(sql, "INSERT INTO LoggedIn(Username) VALUES('%s');", username);

                    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
                    if (rc != SQLITE_OK) {
                        fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, err_msg);
                        sqlite3_free(err_msg);
                        sqlite3_close(db);
                        return -1;
                    }
                } else {
                    printf("[%d]Wrong password for registered user...\n", cp->connfd);
                }
            }

            write(cp->connfd, &loggedIn, sizeof(loggedIn));

            sqlite3_reset(res);
        } while (!loggedIn);
        sqlite3_finalize(res);
        sqlite3_close(db);
    } else {
        int registered = 0;
        do {
            //get username
            read(cp->connfd, username, 100);
            char sql[200] = {0};
            sprintf(sql, "SELECT * FROM Users WHERE Username = '%s';", username);

            char buffer[20] = {0};
            rc = sqlite3_exec(db, sql, callbackRegister, buffer, &err_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, err_msg);
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return -1;
            }

            if (strlen(buffer) == 0) {
                registered = 1;
            }

            write(cp->connfd, &registered, sizeof(registered));
        } while(!registered);

        //get password
        read(cp->connfd, password, 100);

        char sql_register[400] = {0};
        sprintf(sql_register, "INSERT INTO Users(Username, Password) VALUES('%s', '%s');", username, password);

        rc = sqlite3_exec(db, sql_register, 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return -1;
        }

        sqlite3_close(db);

        printf("[%d]Client %d was successfully registered as %s!\n", cp->connfd, cp->connfd, username);
    }

    strcpy(cp->username, username);
}

int fileTransferHandler(char fname[100], struct Client* cp) {
    char filepath[200] = "./books/";
    strcat(filepath, fname);
    write(cp->connfd, fname, 100);

    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL){
        printf("[%d]File open error...\n", cp->connfd);
        return -1;
    }   

    int nread;
    unsigned char buff[2014];
    /* Read data from file and send it */
    while (1) {
        /* First read file in chunks of 256 bytes */
        bzero(buff, 1024);
        nread = fread(buff, 1, 1024, fp);

        /* If read was success, send data. */
        if (nread > 0) {
            //printf("Sending \n");
            write(cp->connfd, buff, nread);
        }
        if (nread < 1024) {
            if (feof(fp)) {
		        printf("[%d]File transfer completed\n", cp->connfd);
		    }
            if (ferror(fp))
                printf("[%d]Error reading\n", cp->connfd);
            break;
        }
    }
    fclose(fp);
}

int callbackAuthor(void *NotUsed, int argc, char **argv, char **azColName) {
    char* bookList = (char*) NotUsed;

    //number, title and author
    strcat(bookList, argv[2]);
    strcat(bookList, "|");
    strcat(bookList, argv[0]);
    strcat(bookList, "|");
    strcat(bookList, argv[1]);
    strcat(bookList, "~");
    
    return 0;
}

int callbackYear(void *NotUsed, int argc, char **argv, char **azColName) {
    char* bookList = (char*) NotUsed;

    //number, title, author and year
    strcat(bookList, argv[3]);
    strcat(bookList, "|");
    strcat(bookList, argv[0]);
    strcat(bookList, "|");
    strcat(bookList, argv[1]);
    strcat(bookList, "|");
    strcat(bookList, argv[2]);
    strcat(bookList, "~");
    
    return 0;
}

int callbackNumber(void *NotUsed, int argc, char **argv, char **azColName) {
    char* bookList = (char*) NotUsed;

    //number, title, author
    strcat(bookList, argv[2]);
    strcat(bookList, "|");
    strcat(bookList, argv[0]);
    strcat(bookList, "|");
    strcat(bookList, argv[1]);
    strcat(bookList, "~");
    
    return 0;
}

int callbackGenre(void *NotUsed, int argc, char **argv, char **azColName) {
    char* bookList = (char*) NotUsed;

    //number, title, author and genre
    strcat(bookList, argv[3]);
    strcat(bookList, "|");
    strcat(bookList, argv[0]);
    strcat(bookList, "|");
    strcat(bookList, argv[1]);
    strcat(bookList, "|");
    strcat(bookList, argv[2]);
    strcat(bookList, "~");
    
    return 0;
}

int handleDownloadList(char bookList[5000], struct Client* cp) {
    char booksReturned[100][300] = {0};

    char* ch = strtok(bookList, "~");
    int i = 0;
    while (ch != NULL) {
        strcat(booksReturned[i], ch);
        ch = strtok(NULL, "~");
        i++;
    }

    char bookNumbers[5000][10] = {0};
    char actualBookList[5000] = {0};

    for(int j = 0; j < i; ++j) {
        char tokensForBook[4][200] = {0};
        char* ch2 = strtok(booksReturned[j], "|");
        int k = 0;
        while (ch2 != NULL) {
            strcat(tokensForBook[k], ch2);
            ch2 = strtok(NULL, "|");
            k++;
        }

        strcat(bookNumbers[j], tokensForBook[0]);

        char num[5];
        sprintf(num, "%d", j + 1);
        strcat(actualBookList, num);
        strcat(actualBookList, ". ");
        strcat(actualBookList, tokensForBook[1]);
        strcat(actualBookList, " by ");
        strcat(actualBookList, tokensForBook[2]);
        if (strlen(tokensForBook[3]) > 0) {
            strcat(actualBookList, " [");
            strcat(actualBookList, tokensForBook[3]);
            strcat(actualBookList, "]");
        }
        strcat(actualBookList, "\n");
    }

    write(cp->connfd, actualBookList, sizeof(actualBookList));
    write(cp->connfd, &i, sizeof(i));

    // Get book download option, if opted for
    char c;
    read(cp->connfd, &c, sizeof(c));
    if (c == 'Y' || c == 'y') {
        int download;
        if(i == 1) {
            download = 1;
        } else {
            read(cp->connfd, &download, sizeof(download));
        }
        if(download > i) {
            download = i;
        }

        char number[10] = "";
        int len;
        i = 1;
        while (bookNumbers[download - 1][i] != '\0') {
            len = strlen(number);
            number[len] = bookNumbers[download - 1][i];
            number[len + 1] = '\0';
            i++;
        }


        //initializing database
        sqlite3 *db;
        char *err_msg = 0;
    
        int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
        if (rc != SQLITE_OK) {    
            fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
            sqlite3_close(db);
            return -1;
        }

        char sql[200] = {0};
        sprintf(sql, "INSERT INTO DownloadedBooks(Username, No) VALUES('%s', '#%s');", cp->username, number);
        
        rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return -1;
        }

        sqlite3_close(db);

        char fname[100] = "pg";
        strcat(fname, number);
        strcat(fname, ".epub");
        printf("[%d]Name of file chosen for download: %s\n", cp->connfd, fname);
        if (fileTransferHandler(fname, cp) == -1) {
            printf("Error transfering file...\n");
            return -1;
        }
    }
}

int listBooksByKeyword(struct Client* cp) {
    //initializing database
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    char keyword[100];
    read(cp->connfd, keyword, 100);

    char bookList[5000] = {0};
    char sql[200] = {0};
    sprintf(sql, "SELECT Title, Author, No FROM Books WHERE Title LIKE '%%%s%%';", keyword);

    rc = sqlite3_exec(db, sql, callbackAuthor, bookList, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    printf("[%d]Listed search results by keyword: %s\n", cp->connfd, keyword);
    
    if (strlen(bookList) > 0) {
        handleDownloadList(bookList, cp);
    } else {
        write(cp->connfd, bookList, sizeof(bookList));
    }
}

int listBooksByCriteria(struct Client* cp) {
    char bookList[5000] = {0};

    //initializing database
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    int advancedSearching = 1;
    while (advancedSearching) {
        bzero(bookList, 5000);

        int searchCriteria;
        read(cp->connfd, &searchCriteria, sizeof(searchCriteria));
        if (searchCriteria == 1) {
            char author[50];
            read(cp->connfd, author, 50);

            char sql[200] = {0};
            sprintf(sql, "SELECT Title, Author, No FROM Books WHERE Author LIKE '%%%s%%';", author);

            rc = sqlite3_exec(db, sql, callbackAuthor, bookList, &err_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return -1;
            }

            printf("[%d]Listed author search results for: %s\n", cp->connfd, author);

            if (strlen(bookList) > 0) {
                handleDownloadList(bookList, cp);
            } else {
                write(cp->connfd, bookList, sizeof(bookList));
            }
        } else if (searchCriteria == 2) {
            int year;
            read(cp->connfd, &year, sizeof(year));

            year /= 10;
            char sql[200] = {0};
            sprintf(sql, "SELECT Title, Author, Year, No FROM Books WHERE Year LIKE '%d_';", year);

            rc = sqlite3_exec(db, sql, callbackYear, bookList, &err_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return -1;
            }

            printf("[%d]Listed year search results for: %d\n", cp->connfd, year);

            if (strlen(bookList) > 0) {
                handleDownloadList(bookList, cp);
            } else {
                write(cp->connfd, bookList, sizeof(bookList));
            }
        } else if (searchCriteria == 3) {
            char number[10];
            read(cp->connfd, number, 10);

            char sql[200] = {0};
            sprintf(sql, "SELECT Title, Author, No FROM Books WHERE No = '%s';", number);

            rc = sqlite3_exec(db, sql, callbackNumber, bookList, &err_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return -1;
            }

            printf("[%d]Listed number search results for: %s\n", cp->connfd, number);

            if (strlen(bookList) > 0) {
                handleDownloadList(bookList, cp);
            } else {
                write(cp->connfd, bookList, sizeof(bookList));
            }
        } else if (searchCriteria == 4) {
            char genre[30];
            read(cp->connfd, genre, 30);

            char sql[200] = {0};
            sprintf(sql, "SELECT Title, Author, Genres, No FROM Books WHERE Genres LIKE '%%%s%%';", genre);

            rc = sqlite3_exec(db, sql, callbackGenre, bookList, &err_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return -1;
            }

            printf("[%d]Listed genre search results for: %s\n", cp->connfd, genre);

            if (strlen(bookList) > 0) {
                handleDownloadList(bookList, cp);
            } else {
                write(cp->connfd, bookList, sizeof(bookList));
            }
        } else if (searchCriteria == 5) {
            float rating;
            read(cp->connfd, &rating, sizeof(rating));

            char sql[200] = {0};
            sprintf(sql, "SELECT Title, Author, AVG(Rating), No FROM Books NATURAL JOIN Ratings GROUP BY No HAVING AVG(Rating) LIKE '%d%%' ORDER BY AVG(Rating) DESC;", (int)rating);

            rc = sqlite3_exec(db, sql, callbackYear, bookList, &err_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return -1;
            }

            printf("[%d]Listed rating search results for: %f\n", cp->connfd, rating);

            if (strlen(bookList) > 0) {
                handleDownloadList(bookList, cp);
            } else {
                write(cp->connfd, bookList, sizeof(bookList));
            }
        } else if (searchCriteria == 6) {
            printf("[%d]Exited advanced search\n", cp->connfd);
            advancedSearching = 0;
        } else {
            printf("[%d]Invalid user search criteria\n", cp->connfd);
        }
    }

    sqlite3_close(db);
}

int listAllBooks(struct Client* cp) {
    //initializing database
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    char bookList[5000] = {0};
    char sql[200] = "SELECT Title, Author, No FROM Books;";

    rc = sqlite3_exec(db, sql, callbackAuthor, bookList, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }
    
    sqlite3_close(db);

    printf("[%d]Listed all books\n", cp->connfd);

    handleDownloadList(bookList, cp);
}

int counter;
int callbackFavourites(void *NotUsed, int argc, char **argv, char **azColName) {
    char* bookList = (char*) NotUsed;

    //index
    char num[10] = {0};
    sprintf(num, "%d", counter);
    strcat(bookList, num);
    strcat(bookList, ". ");
    counter++;

    //title and author
    strcat(bookList, argv[0]);
    strcat(bookList, " by ");
    strcat(bookList, argv[1]);
    strcat(bookList, "\n");
    
    return 0;
}

int handleDownloadedBooks(struct Client* cp) {
    //initializing database
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    char sql[200] = {0};
    sprintf(sql, "SELECT Title, Author, No FROM Books NATURAL JOIN DownloadedBooks WHERE Username = '%s';", cp->username);

    counter = 1;
    char bookList[5000] = {0};
    rc = sqlite3_exec(db, sql, callbackAuthor, bookList, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    char booksReturned[100][300] = {0};

    char* ch = strtok(bookList, "~");
    int i = 0;
    while (ch != NULL) {
        strcat(booksReturned[i], ch);
        ch = strtok(NULL, "~");
        i++;
    }

    char bookNumbers[5000][10] = {0};
    char actualBookList[5000] = {0};

    for(int j = 0; j < i; ++j) {
        char tokensForBook[4][200] = {0};
        char* ch2 = strtok(booksReturned[j], "|");
        int k = 0;
        while (ch2 != NULL) {
            strcat(tokensForBook[k], ch2);
            ch2 = strtok(NULL, "|");
            k++;
        }

        strcat(bookNumbers[j], tokensForBook[0]);

        char num[5];
        sprintf(num, "%d", j + 1);
        strcat(actualBookList, num);
        strcat(actualBookList, ". ");
        strcat(actualBookList, tokensForBook[1]);
        strcat(actualBookList, " by ");
        strcat(actualBookList, tokensForBook[2]);
        strcat(actualBookList, "\n");
    }

    write(cp->connfd, actualBookList, sizeof(actualBookList));
    write(cp->connfd, &i, sizeof(i));

    printf("[%d]Listed downloaded books\n", cp->connfd);

    int action;
    read(cp->connfd, &action, sizeof(action));

    if (action == 1) {
        int favourite;
        if(i == 1) {
            favourite = 1;
        } else {
            read(cp->connfd, &favourite, sizeof(favourite));
        }
        if(favourite > i) {
            favourite = i;
        }

        char number[10] = "";
        int len;
        i = 1;
        while (bookNumbers[favourite - 1][i] != '\0') {
            len = strlen(number);
            number[len] = bookNumbers[favourite - 1][i];
            number[len + 1] = '\0';
            i++;
        }

        char sql[200] = {0};
        sprintf(sql, "INSERT INTO Favourites(Username, No) VALUES('%s', '#%s');", cp->username, number);
            
        rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return -1;
        }
    } else if (action == 2) {
        int rate;
        if(i == 1) {
            rate = 1;
        } else {
            read(cp->connfd, &rate, sizeof(rate));
        }
        if(rate > i) {
            rate = i;
        }

        char number[10] = "";
        int len;
        i = 1;
        while (bookNumbers[rate - 1][i] != '\0') {
            len = strlen(number);
            number[len] = bookNumbers[rate - 1][i];
            number[len + 1] = '\0';
            i++;
        }

        float rating;
        read(cp->connfd, &rating, sizeof(rating));

        char sql[200] = {0};
        sprintf(sql, "INSERT INTO Ratings(Username, No, Rating) VALUES('%s', '#%s', %f);", cp->username, number, rating);

        rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return -1;
        }
    }

    sqlite3_close(db);
}

int listFavourites(struct Client* cp) {
    //initializing database
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    char bookList[5000] = {0};
    char sql[200] = {0};
    counter = 1;
    sprintf(sql, "SELECT Title, Author FROM Favourites NATURAL JOIN Books WHERE Username = '%s';", cp->username);

    rc = sqlite3_exec(db, sql, callbackFavourites, bookList, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    write(cp->connfd, bookList, sizeof(bookList));
    
    sqlite3_close(db);

    printf("[%d]Listed favourite books\n", cp->connfd);
}

int callbackAuthors(void *NotUsed, int argc, char **argv, char **azColName) {
    char* authors = (char*) NotUsed;

    //number, title, author and genre
    strcat(authors, argv[0]);
    strcat(authors, "\n");
    
    return 0;
}

int callbackGenres(void *NotUsed, int argc, char **argv, char **azColName) {
    char* genres = (char*) NotUsed;

    strcat(genres, argv[0]);
    strcat(genres, ",");
    
    return 0;
}

int listAuthorsGenres(struct Client* cp) {
    //initializing database
    sqlite3 *db;
    sqlite3_stmt *res;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    char authors[2000] = {0};
    char sql[200] = {0};
    sprintf(sql, "SELECT DISTINCT Author FROM Books;");

    rc = sqlite3_exec(db, sql, callbackAuthors, authors, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    char authorList[80][50] = {0};

    char* ch = strtok(authors, "\n");
    int i = 0;
    while (ch != NULL) {
        strcat(authorList[i], ch);
        ch = strtok(NULL, "\n");
        i++;
    }

    char authorsGenres[5000] = {0};

    for(i = 0; i < 78; ++i) {
        char sql[200] = {0};
        sprintf(sql, "SELECT DISTINCT Genres FROM Books WHERE Author = '%s';", authorList[i]);

        char genres[100] = {0};
        rc = sqlite3_exec(db, sql, callbackGenres, genres, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[%d]Failed to fetch data: %s\n", cp->connfd, sqlite3_errmsg(db));
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return -1;
        }

        char genreList[30][30] = {0};
        char* genre = strtok(genres, ",");
        int j = 0;
        while (genre != NULL) {
            strcat(genreList[j], genre);
            genre = strtok(NULL, ",");
            j++;
        }

        char distinctGenres[100] = {0};
        for (int k = 0; k < j; ++k) {
            int flag = 1;
            for (int l = 0; l < j, k != l; ++l) {
                if (strcmp(genreList[k], genreList[l]) == 0) {
                    flag = 0;
                }
            }
            if (flag) {
                strcat(distinctGenres, genreList[k]);
                strcat(distinctGenres, ",");
            }
        }

        char distinctGenresFinal[100] = {0};
        strncpy(distinctGenresFinal, distinctGenres, strlen(distinctGenres) - 1);

        strcat(authorsGenres, "- ");
        strcat(authorsGenres, authorList[i]);
        strcat(authorsGenres, " [");
        strcat(authorsGenres, distinctGenresFinal);
        strcat(authorsGenres, "]\n");
    }

    write(cp->connfd, authorsGenres, sizeof(authorsGenres));

    printf("[%d]Listed authors' preferred genres\n", cp->connfd);
}

int callbackDownloaded(void *NotUsed, int argc, char **argv, char **azColName) {
    char* last = (char*) NotUsed;

    strcat(last, argv[0]);
    strcat(last, "|");
    strcat(last, argv[1]);
    strcat(last, "~");
    
    return 0;
}

int callbackSimilar(void *NotUsed, int argc, char **argv, char **azColName) {
    char* similar = (char*) NotUsed;

    strcat(similar, argv[0]);
    strcat(similar, " by ");
    strcat(similar, argv[1]);
    strcat(similar, "\n");
    
    return 0;
}

int callbackUsers(void *NotUsed, int argc, char **argv, char **azColName) {
    char* users = (char*) NotUsed;

    strcat(users, argv[0]);
    strcat(users, "|");
    
    return 0;
}

int listRecommendations(struct Client* cp) {
    //initializing database
    sqlite3 *db;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    //=======1======= RANDOM BOOK OF THE DAY

    char sqls[500] = {0};
    sprintf(sqls, "SELECT Title, Author FROM Books b WHERE NOT EXISTS (SELECT * FROM DownloadedBooks WHERE Username = '%s' AND No = b.No);", cp->username);

    char otherBooks[5000] = {0};
    rc = sqlite3_exec(db, sqls, callbackSimilar, otherBooks, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    char otherBooksList[100][200] = {0};
    char* cho = strtok(otherBooks, "\n");
    int o = 0;
    while (cho != NULL) {
        strcat(otherBooksList[o], cho);
        cho = strtok(NULL, "\n");
        o++;
    }

    srand(time(0));
    int randomBook = rand() % o;

    write(cp->connfd, otherBooksList[randomBook], sizeof(otherBooksList[randomBook]));


    //=======2======= RANDOM SAME GENRE AS LAST DOWNLOADED BOOK

    // GET DOWNLOADED BOOKS
    sprintf(sqls, "SELECT No, Genres FROM DownloadedBooks NATURAL JOIN Books WHERE Username = '%s' ORDER BY Id DESC;", cp->username);

    char dBooks[10];
    rc = sqlite3_exec(db, sqls, callbackDownloaded, dBooks, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    if (strlen(dBooks) > 0) {
        char booksReturned[100][300] = {0};

        char* ch = strtok(dBooks, "~");
        int d = 0;
        while (ch != NULL) {
            strcat(booksReturned[d], ch);
            ch = strtok(NULL, "~");
            d++;
        }

        char downBookNumbers[100][10] = {0};
        char downBookGenres[100][50] = {0};

        for(int j = 0; j < d; ++j) {
            char tokensForBook[4][200] = {0};
            char* ch2 = strtok(booksReturned[j], "|");
            int k = 0;
            while (ch2 != NULL) {
                strcat(tokensForBook[k], ch2);
                ch2 = strtok(NULL, "|");
                k++;
            }

            strcat(downBookNumbers[j], tokensForBook[0]);
            strcat(downBookGenres[j], tokensForBook[1]);
        }

        // GET GENRE LIST OF LAST DOWNLOADED BOOK
        char lastDownGenres[5][20] = {0};
        char* chg = strtok(downBookGenres[0], ",");
        int g = 0;
        while (chg != NULL) {
            strcat(lastDownGenres[g], chg);
            chg = strtok(NULL, ",");
            g++;
        }
        char singleGenre[20] = {0};
        strcat(singleGenre, lastDownGenres[g - 1]);

        // GET A RANDOM BOOK FROM THE LIBRARY THAT'S NOT PART OF THE DOWNLOADS AND SHARES A GENRE WITH THE LAST ONE

        sprintf(sqls, "SELECT Title, Author FROM Books b WHERE GENRES LIKE '%%%s%%' AND NOT EXISTS(SELECT * FROM DownloadedBooks WHERE Username = '%s' AND No = b.No);", singleGenre, cp->username);

        char similarBooks[4000];
        rc = sqlite3_exec(db, sqls, callbackSimilar, similarBooks, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, sqlite3_errmsg(db));
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return -1;
        }

        char similarBooksList[100][200] = {0};
        char* chs = strtok(similarBooks, "\n");
        int s = 0;
        while (chs != NULL) {
            strcat(similarBooksList[s], chs);
            chs = strtok(NULL, "\n");
            s++;
        }

        srand(time(0));
        randomBook = rand() % s;

        write(cp->connfd, similarBooksList[randomBook], sizeof(similarBooksList[randomBook]));
    } else {
        char mess[200] = "No books downloaded yet...";
        write(cp->connfd, mess, sizeof(mess));
    }


    //=======3======= RANDOM BOOK FROM USERS WITH SIMILAR TASTE

    // GET REST OF USERS
    sprintf(sqls, "SELECT Username FROM Users WHERE Username <> '%s';", cp->username);

    char users[300] = {0};
    rc = sqlite3_exec(db, sqls, callbackUsers, users, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    char usersList[100][200] = {0};
    char* chu = strtok(users, "|");
    int u = 0;
    while (chu != NULL) {
        strcat(usersList[u], chu);
        chu = strtok(NULL, "|");
        u++;
    }

    // GET THE USERS THAT HAVE SIMILAR TASTE TO CURRENT USER (AT LEAST 3 BOOKS IN COMMON)
    char similarUsers[100][200] = {0};
    int su = 0;
    for (int j = 0; j < u; ++j) {
        sprintf(sqls, "SELECT COUNT(*) FROM DownloadedBooks d WHERE Username = '%s' AND EXISTS (SELECT * FROM DownloadedBooks WHERE Username = '%s' AND No = d.No);", usersList[j], cp->username);

        char count[5] = {0};
        rc = sqlite3_exec(db, sqls, callbackRegister, count, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, sqlite3_errmsg(db));
            sqlite3_free(err_msg);
            sqlite3_close(db);
            return -1;
        }

        if (strlen(count) > 0) {
            int inCommon = atoi(count);
            if (inCommon >= BOOKS_IN_COMMON) {
                strcat(similarUsers[j], usersList[j]);
                su++;
            }
        }
    }

    if (su != 0) {
        char possibleRecs[100][200] = {0};
        int p;
        for (int j = 0; j < su; ++j) {
            sprintf(sqls, "SELECT Title, Author FROM DownloadedBooks NATURAL JOIN Books d WHERE Username = '%s' AND NOT EXISTS (SELECT * FROM DownloadedBooks WHERE Username = '%s' AND No = d.No);", similarUsers[j], cp->username);
        
            char possibleBooks[4000] = {0};
            rc = sqlite3_exec(db, sqls, callbackSimilar, possibleBooks, &err_msg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, sqlite3_errmsg(db));
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return -1;
            }

            char* chp = strtok(possibleBooks, "\n");
            p = 0;
            while (chp != NULL) {
                strcpy(possibleRecs[p], chp);
                chp = strtok(NULL, "\n");
                p++;
            }
        }

        srand(time(0));
        randomBook = rand() % p;

        write(cp->connfd, possibleRecs[randomBook], sizeof(possibleRecs[randomBook]));
    } else {
        char mess[200] = "Not enough in common with other users...";
        write(cp->connfd, mess, sizeof(mess));
    }

    


    printf("[%d]Listed recommended books\n", cp->connfd);
    sqlite3_close(db);
}

int logOut(struct Client* cp) {
    //initializing database
    sqlite3 *db;
    char *err_msg = 0;
    
    int rc = sqlite3_open("/home/cristian/Desktop/Server-Client Tests/server/databases/main.db", &db);
    if (rc != SQLITE_OK) {    
        fprintf(stderr, "[%d]Cannot open database: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    char sql[200] = {0};
    sprintf(sql, "DELETE FROM LoggedIn WHERE Username = '%s';", cp->username);

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%d]SQL error: %s\n", cp->connfd, sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }

    printf("Closing Connection for id: %d\n", cp->connfd);
}

int clientHandler(struct Client* cp) {
    printf("Connection accepted and id: %d\n", cp->connfd);
    printf("Connected to Client: %s:%d\n", inet_ntoa(cp->addr.sin_addr), ntohs(cp->addr.sin_port));
    
    if (authenticationHandler(cp) == -1) {
        printf("Error authenticating client...\n");
        return -1;
    }

    int loggedIn = 1;
    while (loggedIn) {
        int option;
        read(cp->connfd, &option, sizeof(option));

        if (option == 1) {
            if (listBooksByKeyword(cp) == -1) {
                printf("[%d]Error transfering file...\n", cp->connfd);
                //return -1;
            }
        } else if (option == 2) {
            if (listBooksByCriteria(cp) == -1) {
                printf("[%d]Error listing books by criteria...\n", cp->connfd);
                //return -1;
            }
        } else if (option == 3) {
            if (listAllBooks(cp) == -1) {
                printf("[%d]Error listing books...\n", cp->connfd);
                //return -1;
            }
        } else if (option == 4) {
            if (handleDownloadedBooks(cp) == -1) {
                printf("[%d]Error listing downloaded books...\n", cp->connfd);
                //return -1;
            }
        } else if (option == 5) {
            if (listFavourites(cp) == -1) {
                printf("[%d]Error listing favourites...\n", cp->connfd);
                //return -1;
            }
        } else if (option == 6) {
            if (listAuthorsGenres(cp) == -1) {
                printf("[%d]Error listing authors' genres...\n", cp->connfd);
                //return -1;
            }
        } else if (option == 7) {
            if (listRecommendations(cp) == -1) {
                printf("[%d]Error listing recommendations...\n", cp->connfd);
                //return -1;
            }
        } else if (option == 8) {
            if (logOut(cp) == -1) {
                printf("[%d]Error logging out...\n", cp->connfd);
                return -1;
            }
            loggedIn = 0;
        } else {
            printf("[%d]Invalid user option\n", cp->connfd);
        }
    }
}

static void* treat(void* arg) {
    struct Client cp = *((struct Client*)arg);
    // printf("[thread]-%d...\n", cp.tid);
    // fflush(stdout);
    // pthread_detach(pthread_self());

    if (clientHandler(&cp) == -1) {
        printf("[%d]Client handling error...\n", cp.connfd);
        return (void*)-1;
    }

    close(cp.connfd);
    return(NULL);
}

int main (int argc, char *argv[]) {

    //setting up server
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd;
    int ret;
    pthread_t th[100];
    int i = 0;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
	    printf("Error in socket creation\n");
	    exit(2);
	}

    printf("Socket retrieve success\n");

    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    ret = bind(sd, (struct sockaddr*)&server, sizeof(server));
    if (ret < 0) {
        printf("Error in bind\n");
        exit(2);
    }

    if (listen(sd, 3) == -1) {
        printf("Failed to listen\n");
        return -1;
    }

    while (1) {
        struct Client* cp;
        int connfd;
        size_t len = sizeof(from);
        printf("Waiting on port %d...\n", PORT);
        fflush(stdout);

        connfd = accept(sd, (struct sockaddr*)&from, (socklen_t*)&len);
        if (connfd < 0) {
			printf("Error in accept...\n");
			continue;
		}

        if (i > 9) {
            i = 0;
        }

        cp = (struct Client*)malloc(sizeof(struct Client));
        cp->connfd = connfd;
        cp->tid = i++;
        cp->addr = from;

        err = pthread_create(&th[i], NULL, &treat, cp);
        if (err != 0) {
            printf("\nCan't create thread :[%s]", strerror(err));
        }
	}
    
    return 0;
}