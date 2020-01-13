# Reads-Profiler

Create a client-server application through which a client can access an online
library. The books should be able to be searched by several criteria(genre, author,
title, year, ISBN, rating) and to be downloadable by the client. A hierarchy of
genres and subgenres will be put in place and each book will belong to one or
multiple genres/subgenres of it. Also, an author will have his preferred genres
specified.

The server will keep track of the searches and the results accessed by the
client for a certain search, as well as the downloads made. Based on this data(a
book’s genres/subgenres, an author’s preferred genres, a book’s rating, a client’s
searches and downloads), the server will be able to make recommendations to
the client. As client activity grows, the recommendations system will improve
in accuracy, considering factors such as a client’s taste, preferences of clients
with similar taste and ratings of books given by clients to which they were rec-
ommended.

## To run server and client, use Makefile:

**make -f MakefileServer** for server application
**make -f MakefileClient** for client application

## Or, alternatively: 

**./server** for server application
**./client** OR **./client IP PORT** for client application


## Most user interaction is done through a menu

After establishing connection, at the start of the client application, the user is prompted for login/register with corresponding select options as either 1 or 2. After successfull login/register, the main user actions are listed as options ranging from 1 to 8, 8 being for logging out.

Most of the actions produce a book list that are followed with a Y/N prompt for whether the user wants to download one the listed books or not.
