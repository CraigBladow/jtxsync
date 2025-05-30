
/*********************************************************************
jtxsync version 0.3

Copyright (C) 2025 Craig Bladow, K0CWB

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#define WSJTX_PORT 2237
#define MAX_BUF_SIZE 2048
#define DEFAULT_NUM_SAMPLES 10
#define MAX_SAMPLES 100
#define MIN_SAMPLES 4

#define DEBUG 0  // Set to 1 to display debug messages

uint32_t exit_commanded = 0; // Setting to not 0 causes program to exit
uint32_t max_samples = 10;   // default setting for max_samples

void usage(void)
{
    printf("Usage:\n");
    printf("jtsync launches with a default of 10 delta time (DT) samples.\n");
    printf("For more or less samples use jtsync -n ### where ### is a number from %d to %d.\n",MIN_SAMPLES,MAX_SAMPLES);
}

double network_buffer_to_double(const unsigned char *buffer)
{
    double value;
    unsigned char temp_buffer[sizeof(double)];

    // Copy the buffer to a temporary buffer to avoid modifying the original
    memcpy(temp_buffer, buffer, sizeof(double));

    // Check if the system's endianness is different from network byte order (big-endian)
    int num = 1;
    if (*(char *)&num == 1)
    {
        // Little-endian system, reverse the byte order
        for (size_t i = 0; i < sizeof(double) / 2; ++i)
        {
            unsigned char temp = temp_buffer[i];
            temp_buffer[i] = temp_buffer[sizeof(double) - 1 - i];
            temp_buffer[sizeof(double) - 1 - i] = temp;
        }
    }

    // Copy the (potentially reordered) bytes into the double variable
    memcpy(&value, temp_buffer, sizeof(double));
    return value;
}

void print_current_time(char *prompt)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *ptm = localtime(&tv.tv_sec);
    printf("%s: %02d:%02d:%02d.%03d\n", prompt, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, tv.tv_usec / 1000);
    return;
}

/*
 * Adjusts the system clock by a specified delta time.
 * returns 0 on success, -1 on failure (and sets errno).
 */
int adjustSystemClock(double delta_time)
{
    struct timeval current_time;
    struct timeval new_time;
    long seconds;
    long microseconds;

    char ans[16];
    time_t t;
    // Get the current system time
    if (gettimeofday(&current_time, NULL) == -1)
    {
        perror("Error getting current time");
        return -1;
    }

    // Convert delta_time (double) into seconds and microseconds
    seconds = (long)delta_time;
    microseconds = (long)((delta_time - seconds) * 1000000.0);

    // Calculate the new time
    new_time.tv_sec = current_time.tv_sec + seconds;
    new_time.tv_usec = current_time.tv_usec + microseconds;

    // Handle potential microsecond overflow/underflow
    if (new_time.tv_usec >= 1000000)
    {
        new_time.tv_sec++;
        new_time.tv_usec -= 1000000;
    }
    else if (new_time.tv_usec < 0)
    {
        new_time.tv_sec--;
        new_time.tv_usec += 1000000;
    }
    // Print the time before adjustment
    print_current_time("Current time");
    // Set the new system time
    if (settimeofday(&new_time, NULL) == -1)
    {
        fprintf(stderr, "Error setting new time: %s\n", strerror(errno));

        if (errno == EPERM)
        {
            fprintf(stderr, "Permission denied. You might need to run this program as root (e.g., using 'sudo').\n");
        }
        return -1;
    }
    else
        print_current_time("Adjusted time");

    printf("System clock adjusted successfully by %lf seconds.\n", delta_time);
    printf("Do you want to quit jtxsync?  (Y)es, (N)o ?\n");
    fgets(ans, 16, stdin);
    if (ans[0] == 'Y' || ans[0] == 'y')
        exit_commanded = 1;
    return 0;
}

// Time adjustment calculations
uint32_t sample_i;
double sample_array[MAX_SAMPLES];

// sample standard deviation and mean
double std_deviation(double data[], uint32_t data_len, double *mean)
{
    double sum = 0.0, std_dev = 0.0, sum_sqrs = 0.0, variance = 0.0;
    int i;
    if (data_len > MAX_SAMPLES)
        data_len = MAX_SAMPLES;
    for (i = 0; i < data_len; i++)
        sum += data[i];
    *mean = sum / data_len;
    for (i = 0; i < data_len; i++)
        sum_sqrs += pow(data[i] - *mean, 2);
    variance = sum_sqrs / (data_len-1);
    std_dev = sqrt(variance);
    return std_dev;
}
void init_delta_time_accum(void)
{
    printf("Initializing time correction calculation.\n");
    sample_i = 0;
    memset(sample_array, 0, sizeof(sample_array));
}

void delta_time_accum(double sample)
{
    int i;
    double sdev = 0.0, mean = 0.0;

    // Accumulate max_samples samples
    printf("sample %u: %f \n", sample_i, sample);
    sample_array[sample_i] = sample;
    sample_i += 1;

    // Update every max_samples
    if (sample_i >= max_samples)
    {
        sample_i = 0;
        double new_mean_time;
        char ans[16];
        int new_mean_count = 0;
        double new_mean_sum = 0.0;
        double upper_bound, lower_bound;
        

        sdev = std_deviation(&sample_array[0], max_samples, &mean);
        if (DEBUG)
            printf("sdev: %f mean: %f\n", sdev, mean);

        // Calculate new mean from samples that fall within mean +/- 1 sdev
        upper_bound = mean + sdev;
        lower_bound = mean - sdev;
        printf("Ignoring samples less than %f seconds and greater than %f seconds.\n", lower_bound,upper_bound);
        for (i = 0; i < max_samples; i++)
        {
            //if (fabs(sample_array[i]) <= fabs(mean + sdev))
            if(sample_array[i]>lower_bound && sample_array[i]<upper_bound)
            {
                new_mean_sum += sample_array[i];
                new_mean_count++;
            }
        }
        if (DEBUG)
            printf("new_mean_count %u\n", new_mean_count);
        if(new_mean_count >= 1)
        {
            new_mean_time = -1.0 * (new_mean_sum / (new_mean_count));

            printf("Adjust system clock by %f seconds? (Y)es, (N)o or (Q)uit?\n", new_mean_time);
            fgets(ans, 16, stdin);

            if (ans[0] == 'Y' || ans[0] == 'y')
            {
                if (adjustSystemClock(new_mean_time) == -1)
                {
                    printf("Do you want to quit jtxsync?  (Y)es, (N)o ?\n");
                    fgets(ans, 16, stdin);
                    if (ans[0] == 'Y' || ans[0] == 'y')
                        exit_commanded = 1;
                }
            }
            else
            {
                printf("Skipping clock adjustment.\n");
                if (ans[0] == 'Q' || ans[0] == 'q')
                    exit_commanded = 1;
            }
        }
        else
        {
            printf("Calculation failed, Do you want to try again?  (Y)es, (N)o ?\n");
            fgets(ans, 16, stdin);
            if (ans[0] == 'N' || ans[0] == 'n')
                exit_commanded = 1;
        }
    }
}

// WSJT-X message header structure
typedef struct
{
    uint32_t magic;
    uint32_t schema;
    uint32_t msg_id;
} wsjtx_header_t;

typedef struct
{
    wsjtx_header_t header;
    char *payload;
} wsjtx_message_t;

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    char buffer[MAX_BUF_SIZE];
    int i;

    uint32_t status_receiving = 0; // Inital one time receive success message sets to 1
    if (argc == 1)
        max_samples = DEFAULT_NUM_SAMPLES;
    else
    {
        for (i = 1; i < argc; i++)
        {
            if (argv[i][0] == '-')
            {
                switch (argv[i][1])
                {
                // printf("arg[i][1]:%c\n",argv[i][1]);  // development debug

                // Set custom sample number
                case 'n':
                case 'N':
                    i++;

                    max_samples = atoi((char *)argv[i]);
                    if (max_samples < MIN_SAMPLES)
                        max_samples = MIN_SAMPLES;
                    else if (max_samples > MAX_SAMPLES)
                        max_samples = MAX_SAMPLES;

                    break;

                default:
                    fprintf(stderr, "%s: unknown arg %s\n",
                            argv[0], argv[i]);
                    usage();
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    // Welcome
    printf("Welcome to jtxsync development version 0.3 by K0CWB.\n");
    printf("Copyright (C) 2025 Craig Bladow.  All rights reserved.\n");
    printf("This is free software; see the LICENSE file for copying conditions.\nThere is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
    usage();
    printf("Using %d samples for calculations.\n", max_samples);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Server address configuration
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(WSJTX_PORT);

    // Bind socket to address
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on Localhost port %d...\n", WSJTX_PORT);

    init_delta_time_accum();

    while (exit_commanded == 0)
    {
        len = sizeof(cliaddr);
        int n = recvfrom(sockfd, (char *)buffer, MAX_BUF_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0)
        {
            perror("recvfrom failed");
            continue; // Continue listening for messages
        }

        buffer[n] = '\0';
        wsjtx_message_t *message = (wsjtx_message_t *)buffer;

        // Check for WSJT-X magic number, schema and id.
        if (ntohl(message->header.magic) == 0xadbccbda)
        {
            uint32_t schema = ntohl(message->header.schema);
            uint32_t msg_id = ntohl(message->header.msg_id);
            if (DEBUG)
                printf("Received WSJT-X message schema: %u msg_id: %u ", schema, msg_id);

            // Handle schemas other than 2
            if (schema != 2)
            {
                printf("    Can't decode schema: %u\n", schema);
            }
            else if (msg_id != 2)
            {
                // Ignore if not id = 2, DECODE
                if (DEBUG)
                    printf("   Ignoring Message\n");
            }
            else
            {
                /* WSJT-X message structure for schema 2, Decode message 
                                         Unique Id Length       uint32  4 bytes                       
                                         Id (unique key)        utf8      'WSJT-X'  6 bytes
                                         New                    bool     1 bytes
                                         Time                   Time     3 bytes
                                         snr                    int32    4 bytes
                                         Delta time (S)         double   8 bytes
                                         Delta frequency (Hz)   uint32   4 bytes
                                         Mode                   utf8     tbd
                                         Message                utf8     tbd
                                         Low confidence         bool     tbd
                */

                // Process schema 2 decode message
                uint32_t count = 12; // skipping header bytes
                uint32_t uid_len = 0;
                uint32_t i = 0;
                double delta_time;

                //if (DEBUG) printf("Received WSJT-X message, schema: %u msg_id: %u", schema, msg_id);

                // Decode Unique Id it should be WSJT-X
                uid_len = (buffer[count++] << 24);
                uid_len |= (buffer[count++] << 16);
                uid_len |= (buffer[count++] << 8);
                uid_len |= (buffer[count++]);

                char uid[16];
                if (uid_len > 15)
                    uid_len = 15; // Don't let the length exceed uid[] size less one.
                if (DEBUG)
                    printf("    Unique ID len: %u ", uid_len);
                for (i = 0; i < uid_len; i++) uid[i] = buffer[count++];
                uid[i] = '\0';
                count++;  // makes this work?
                
                if (DEBUG)
                    printf("    Unique ID: %s\n", uid); // it's WSJT-X !
                // Continue decode of unique ID is WSJT-X
                if (strcmp(uid, "WSJT-X") == 0)
                {
                    // Inital one time receive success message
                    if (status_receiving == 0)
                    {
                        printf("Receiving Decode message(s) from WSJT-X\n");
                        status_receiving = 1;
                    }

                    // Decode new bool - not needed
                    //char new_bool = buffer[count++];
                    //if (DEBUG) printf("    new_bool %d\n", new_bool);
                    // Skip new bool
                    count += 1;

                    // Skip three byte time value as not used
                    count += 3; 

                    int32_t snr = ((int32_t)buffer[count++] << 24);
                    snr |= ((int32_t)buffer[count++] << 16);
                    snr |= ((int32_t)buffer[count++] << 8);
                    snr |= ((int32_t)buffer[count++]);
                    if (DEBUG)
                        printf("    snr: %d\n", snr);

                    // Decode delta time
                    char dtbuf[8];
                    for (i = 0; i < 8; i++) dtbuf[i] = buffer[count + i];
                    delta_time = network_buffer_to_double((const unsigned char *)dtbuf);
                    delta_time_accum(delta_time);
                    count += 8;// Move past delta time double that was just decoded
                    if (DEBUG) printf("    Decoded delta time: %f\n", delta_time);
                }
                else if (DEBUG)
                    printf("!Receiving traffic from other source %s\n", uid);
            }
        }
    } // End while(exit_commanded == 0)
    printf("Thank you for using jtxsync!\n");
    close(sockfd);
    return 0;
}
