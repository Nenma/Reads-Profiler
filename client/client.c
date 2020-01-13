#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>

int sockfd = 0;
int bytesReceived = 0;
char recvBuff[1024];

int establishConnection(int argc, char *argv[]) {

    memset(recvBuff, '0', sizeof(recvBuff));
    struct sockaddr_in serv_addr;

    /* Create a socket first */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Error : Could not create socket \n");
        return -1;
    }

    char ip[50];
    int port;
    if (argc < 3) {
        printf("Enter IP address to connect: ");
        scanf("%s", ip);
        printf("Enter port to connect to: ");
        scanf("%d", &port);
    }
    else {
        strcpy(ip, argv[1]);
        port = atoi(argv[2]);
    }

    /* Initialize sockaddr_in data structure */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip);

    /* Attempt a connection */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n Error : Connect Failed \n");
        return -1;
    }

    printf("Connected to IP: %s:%d\n", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
}

void authenticationHandler() {

    char username[100], password[100];

    printf("Please login or register before using the services:\n");
    printf("1 - Login\n");
    printf("2 - Register\n");

    int option;
    printf("Your option: ");
    scanf("%d", &option);
    write(sockfd, &option, sizeof(option));

    if (option == 1) {
        int loggedIn = 0;
        do {
            int alreadyLoggedIn = 1;
            do {
                printf("Username: ");
                scanf("%s", username);
                write(sockfd, username, sizeof(username));

                read(sockfd, &alreadyLoggedIn, sizeof(alreadyLoggedIn));
                if (alreadyLoggedIn == 1) {
                    printf("That user is already logged in!\n");
                }
            } while (alreadyLoggedIn);

            printf("Password: ");
            scanf("%s", password);
            write(sockfd, password, sizeof(password));

            read(sockfd, &loggedIn, sizeof(loggedIn));
            if(loggedIn) {
                printf("You have successfully logged in! Welcome back, %s!\n", username);
            } else {
                printf("Incorrect username or password! Try again.\n");
            }
        } while (!loggedIn);
    } else {
        int registered = 0;
        do {
            printf("Please enter your prefered username: ");
            scanf("%s", username);
            write(sockfd, username, sizeof(username));
            
            read(sockfd, &registered, sizeof(registered));
            if (registered == 0) {
                printf("That username is already taken! Choose another.\n");
            }
        } while (!registered);
        
        printf("Please choose a password: ");
        scanf("%s", password);
        write(sockfd, password, sizeof(password));

        printf("Welcome %s! You have been successfully registered!\n", username);
    }
}

int fileTransferHandler() {
    /* Create file where data will be stored */
	char fname[100];

	read(sockfd, fname, sizeof(fname));
	
	printf("Receiving file %s...\n", fname);

    FILE *fp;
   	fp = fopen(fname, "ab"); 
    if (NULL == fp){
       	printf("Error opening file...\n");
        return -1;
    }
    long double sz = 0;
    /* Receive data in chunks of 256 bytes */
    while (1) {
        bytesReceived = read(sockfd, recvBuff, 1024);
        sz++;
	    fflush(NULL);
        fwrite(recvBuff, 1, bytesReceived, fp);
        if (bytesReceived < 1024) {
            sleep(1);
            break;
        }
    }
    //bytesReceived = read(sockfd, recvBuff, 1024);
    //fwrite(recvBuff, 1, bytesReceived, fp);

    printf("Received: %Lf Mb\n", (sz / 1024));

    if (bytesReceived < 0) {
        printf("Read Error \n");
    }
    printf("File OK....Completed\n");

    fclose(fp);
}

int handleDownloadList() {
    int listSize;
    read(sockfd, &listSize, sizeof(listSize));

    char c;
    printf("Would you like to download one of these?[Y/N]\n");
    scanf(" %c", &c);
    write(sockfd, &c, sizeof(c));

    if (c == 'Y' || c == 'y') {
        int download;
        if (listSize == 1) {
            download = 1;
        } else {
            printf("Your option: ");
            scanf("%d", &download);
            write(sockfd, &download, sizeof(download));
        }
        if (fileTransferHandler() == -1) {
            printf("Error transfering file...\n");
            return -1;
        }
    } else {
        printf("Continue to browse at your leisure!\n");
    }
}

int main (int argc, char *argv[]) {

    system("clear");
    
    //connecting to the server
    if (establishConnection(argc, argv) == -1) {
        printf("Error establishing connection...\n");
        return -1;
    }

    //login and registration handling
    authenticationHandler();

    int loggedIn = 1;
    while (loggedIn) {
        printf("This is the Reads Profiler application! What would you like to do?\n");
        printf("1 - Search the library\n");
        printf("2 - Advanced search\n");
        printf("3 - List all books\n");
        printf("4 - List downloaded books\n");
        printf("5 - List favourites\n");
        printf("6 - See each author's preferred genres\n");
        printf("7 - See recommandations\n");
        printf("8 - Exit\n");

        int option;
        printf("Your option: ");
        scanf("%d", &option);
        write(sockfd, &option, sizeof(option));

        if(option == 1) {
            char keyword[100];
            printf("Enter keyword to search book titles: ");
            scanf("%s", keyword);
            write(sockfd, keyword, sizeof(keyword));

            char bookListByKeyword[5000] = {0};
            read(sockfd, bookListByKeyword, 5000);
            printf("%s\n", bookListByKeyword);

            if(strlen(bookListByKeyword) > 0) {
                if (handleDownloadList() == -1) {
                    printf("Error handling download request...\n");
                    return -1;
                }
            } else {
                printf("There are no results...\n");
            }
        } else if (option == 2) {
            int advancedSearching = 1;
            while (advancedSearching) {
                printf("Below are the search criteria options:\n");
                printf("1 - by Author\n");
                printf("2 - by Year\n");
                printf("3 - by Number\n");
                printf("4 - by Genre\n");
                printf("5 - by Rating\n");
                printf("6 - Exit\n");

                int searchCriteria;
                printf("Your option: ");
                scanf("%d", &searchCriteria);
                write(sockfd, &searchCriteria, sizeof(searchCriteria));

                if (searchCriteria == 1) {
                    char author[50];
                    char bookListByAuthor[5000];

                    printf("Enter author's name: ");
                    scanf("%s", author);
                    write(sockfd, author, sizeof(author));

                    read(sockfd, bookListByAuthor, 5000);

                    if(strlen(bookListByAuthor) > 0) {
                        printf("%s\n", bookListByAuthor);
                        if (handleDownloadList() == -1) {
                            printf("Error handling download request...\n");
                            return -1;
                        }
                    } else {
                        printf("There are no results...\n");
                    }
                } else if (searchCriteria == 2) {
                    int year;
                    char bookListByYear[5000];

                    printf("Enter year: ");
                    scanf("%d", &year);
                    write(sockfd, &year, sizeof(year));

                    read(sockfd, bookListByYear, 5000);

                    if(strlen(bookListByYear) > 0) {
                        printf("%s\n", bookListByYear);
                        if (handleDownloadList() == -1) {
                            printf("Error handling download request...\n");
                            return -1;
                        }
                    } else {
                        printf("There are no results...\n");
                    }
                } else if (searchCriteria == 3) {
                    char number[10];
                    char bookByNumber[5000];

                    printf("Enter book's number: ");
                    scanf("%s", number);
                    write(sockfd, number, sizeof(number));

                    read(sockfd, bookByNumber, 5000);

                    if(strlen(bookByNumber) > 0) {
                        printf("%s\n", bookByNumber);
                        if (handleDownloadList() == -1) {
                            printf("Error handling download request...\n");
                            return -1;
                        }
                    } else {
                        printf("There are no results...\n");
                    }
                } else if (searchCriteria == 4) {
                    char genres[30][30] = {"Fiction", "Love", "Horror", "Short", "History", "Politics", "Adventure", "Humour", "War", "Drama", "Biography", "Mystery", "Comedy", "Children", "Christmas", "Ancient", "Poetry", "Epic", "Fantasy", "Philosophy", "Education", "Legends"};
                    printf("Here is a list of genres you can pick from:\n");
                    for (int i = 0; i < 22; ++i) {
                        printf("%d - %s\n", i + 1, genres[i]);
                    }
                    
                    int genre;
                    printf("Your option: ");
                    scanf("%d", &genre);

                    if (genre == 1) write(sockfd, genres[0], sizeof(genres[0]));
                    else if (genre == 2) write(sockfd, genres[1], sizeof(genres[1]));
                    else if (genre == 3) write(sockfd, genres[2], sizeof(genres[2]));
                    else if (genre == 4) write(sockfd, genres[3], sizeof(genres[3]));
                    else if (genre == 5) write(sockfd, genres[4], sizeof(genres[4]));
                    else if (genre == 6) write(sockfd, genres[5], sizeof(genres[5]));
                    else if (genre == 7) write(sockfd, genres[6], sizeof(genres[6]));
                    else if (genre == 8) write(sockfd, genres[7], sizeof(genres[7]));
                    else if (genre == 9) write(sockfd, genres[8], sizeof(genres[8]));
                    else if (genre == 10) write(sockfd, genres[9], sizeof(genres[9]));
                    else if (genre == 11) write(sockfd, genres[10], sizeof(genres[10]));
                    else if (genre == 12) write(sockfd, genres[11], sizeof(genres[11]));
                    else if (genre == 13) write(sockfd, genres[12], sizeof(genres[12]));
                    else if (genre == 14) write(sockfd, genres[13], sizeof(genres[13]));
                    else if (genre == 15) write(sockfd, genres[14], sizeof(genres[14]));
                    else if (genre == 16) write(sockfd, genres[15], sizeof(genres[15]));
                    else if (genre == 17) write(sockfd, genres[16], sizeof(genres[16]));
                    else if (genre == 18) write(sockfd, genres[17], sizeof(genres[17]));
                    else if (genre == 19) write(sockfd, genres[18], sizeof(genres[18]));
                    else if (genre == 20) write(sockfd, genres[19], sizeof(genres[19]));
                    else if (genre == 21) write(sockfd, genres[20], sizeof(genres[20]));
                    else if (genre == 22) write(sockfd, genres[21], sizeof(genres[21]));

                    char bookListByGenre[5000];
                    read(sockfd, bookListByGenre, 5000);

                    if(strlen(bookListByGenre) > 0) {
                        printf("%s\n", bookListByGenre);
                        if (handleDownloadList() == -1) {
                            printf("Error handling download request...\n");
                            return -1;
                        }
                    } else {
                        printf("There are no results...\n");
                    }
                } else if (searchCriteria == 5) {
                    float rating;
                    char bookListByRating[5000];

                    printf("Enter rating: ");
                    scanf("%f", &rating);
                    write(sockfd, &rating, sizeof(rating));

                    read(sockfd, bookListByRating, 5000);

                    if(strlen(bookListByRating) > 0) {
                        printf("%s\n", bookListByRating);
                        if (handleDownloadList() == -1) {
                            printf("Error handling download request...\n");
                            return -1;
                        }
                    } else {
                        printf("There are no results...\n");
                    }
                } else if (searchCriteria == 6){
                    printf("Returning...\n");
                    advancedSearching = 0;
                } else {
                    printf("That is not a valid option! Try again.\n");
                }
            }
        } else if (option == 3) {
            char bookList[5000];
            read(sockfd, bookList, 5000);
            printf("%s\n", bookList);

            if (handleDownloadList() == -1) {
                printf("Error handling download request...\n");
                return -1;
            }
        } else if (option == 4) {
            char downloadedBooks[5000];
            read(sockfd, downloadedBooks, 5000);

            int listSize;
            read(sockfd, &listSize, sizeof(listSize));

            if (strlen(downloadedBooks) > 0) {
                printf("These are your books!\n");
                printf("%s\n", downloadedBooks);

                printf("These are the actions available here:\n");
                printf("1 - Add to Favourites\n");
                printf("2 - Give rating\n");
                printf("3 - Exit\n");

                int action;
                printf("Your option: ");
                scanf("%d", &action);
                write(sockfd, &action, sizeof(action));

                if (action == 1) {
                    int favourite;
                    if (listSize == 1) {
                        favourite = 1;
                    } else {
                        printf("Which book would like to add to your favourites?\n");
                        printf("Your option: ");
                        scanf("%d", &favourite);
                        write(sockfd, &favourite, sizeof(favourite));
                    }

                    printf("Done!\n");
                } else if (action == 2) {
                    int rate;
                    if (listSize == 1) {
                        rate = 1;
                    } else {
                        printf("Which book would like to rate?\n");
                        printf("Your option: ");
                        scanf("%d", &rate);
                        write(sockfd, &rate, sizeof(rate));
                    }

                    float rating;
                    printf("What is your rating of this book? (1.0 - 5.0)\n");
                    printf("Your rating: ");
                    scanf("%f", &rating);
                    write(sockfd, &rating, sizeof(rating));

                    printf("Done!\n");
                } else {
                    printf("Returning...\n");
                }
            } else {
                printf("There are no results...\n");
            }
        } else if (option == 5) {
            char favouriteBooks[5000];
            read(sockfd, favouriteBooks, 5000);

            if (strlen(favouriteBooks) > 0) {
                printf("%s\n", favouriteBooks);
            } else {
                printf("There are no results...\n");
            }
        } else if (option == 6) {
            char authorsGenres[5000];
            read(sockfd, authorsGenres, 5000);
            printf("%s\n", authorsGenres);
        } else if (option == 7) {
            char randomPick[200] = {0};
            read(sockfd, randomPick, sizeof(randomPick));
            printf("Random pick from our library: %s\n", randomPick);

            char similarGenreBook[200] = {0};
            read(sockfd, similarGenreBook, sizeof(similarGenreBook));
            printf("Based on recent book's genre: %s\n", similarGenreBook);

            char similarInterestBook[200] = {0};
            read(sockfd, similarInterestBook, sizeof(similarInterestBook));
            printf("Users with similar taste also liked: %s\n", similarInterestBook);
        }
        else if (option == 8) {
            printf("You have logged out. Come back soon!\n");
            loggedIn = 0;
        } else {
            printf("That is not a valid option! Try again.\n");
        }
    }

    close(sockfd);
   	 
    return 0;
}