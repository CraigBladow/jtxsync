// TODO does this work on ARM?
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>

#define WSJTX_PORT 2237
#define MAX_BUF_SIZE 1024

#define DEBUG 0

double network_buffer_to_double(const unsigned char *buffer) {
    double value;
    unsigned char temp_buffer[sizeof(double)];

    // Copy the buffer to a temporary buffer to avoid modifying the original
    memcpy(temp_buffer, buffer, sizeof(double));

    // Check if the system's endianness is different from network byte order (big-endian)
    int num = 1;
    if (*(char *)&num == 1) {
        // Little-endian system, reverse the byte order
        for (size_t i = 0; i < sizeof(double) / 2; ++i) {
            unsigned char temp = temp_buffer[i];
            temp_buffer[i] = temp_buffer[sizeof(double) - 1 - i];
            temp_buffer[sizeof(double) - 1 - i] = temp;
        }
    }

    // Copy the (potentially reordered) bytes into the double variable
    memcpy(&value, temp_buffer, sizeof(double));
    return value;
}

#define MAX_SAMPLES 20
#define MIN_SAMPLES 10
uint32_t sample_count,sample_next;
double sample_array[MAX_SAMPLES];


// standard deviation and mean
double std_deviation(double data[], uint32_t data_len, double *mean) 
{

    double sum = 0.0, std_dev = 0.0,sum_sqrs=0.0,variance = 0.0;
    int i;
    for (i = 0; i < data_len; i++) sum += data[i];
    *mean = sum / data_len;
    for (i = 0; i < data_len; i++) sum_sqrs += pow(data[i] - *mean, 2);
    variance = sum_sqrs/data_len;
    std_dev = sqrt(variance);
    return std_dev;
}

void print_results(double sdev, double mean, uint32_t number_of_samples_to_print)
{
    int i;
    printf("sdev: %f mean: %f",sdev, mean);
    printf("samples: ");
    for(i=0;i< number_of_samples_to_print;i++)
    {
        printf("%f,",sample_array[i]);
        
    }
    printf("\n");
    

}

void init_delta_time_accum(void)
{
    printf("Initializing Time Correction Calculation.\n");
    sample_count = 0;
    sample_next = 0;
    memset(sample_array, 0, sizeof(sample_array));
}

void delta_time_accum(double sample)
{
    double sdev = 0.0, mean=0.0;
    // accumulate samples in rotating buffer
    // minimum of 10 
    sample_array[sample_next]=sample;

    if(++sample_next >= MAX_SAMPLES) sample_next = 0;
    if(sample_count < MAX_SAMPLES-1) sample_count++;
    printf("sample: %f sample_count: %u\n",sample,sample_count);

    // update every 10 samples
    
    if(sample_count >= MIN_SAMPLES)
    { 
        sdev = std_deviation(&sample_array[0], sample_count, &mean); 
        print_results(sdev,mean, sample_count);
    }

}




// WSJT-X message header structure
typedef struct {
    uint32_t magic;
    uint32_t schema;
    uint32_t msg_id;
} wsjtx_header_t;


typedef struct {
    wsjtx_header_t header;
    char *payload;
} wsjtx_message_t;



int main() 
{
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    char buffer[MAX_BUF_SIZE];



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

    printf("Listening on port %d...\n", WSJTX_PORT);

    init_delta_time_accum();

    while (1) 
    {
        len = sizeof(cliaddr);
        int n = recvfrom(sockfd, (char *)buffer, MAX_BUF_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom failed");
            continue; // Continue listening for messages
        }

        buffer[n] = '\0';
        wsjtx_message_t *message = (wsjtx_message_t *)buffer;

        // Check for WSJT-X magic number and schema
        if (ntohl(message->header.magic) == 0xadbccbda) {
            uint32_t schema = ntohl(message->header.schema);
            uint32_t msg_id = ntohl(message->header.msg_id);
            if(DEBUG) printf("Received WSJT-X message header schema: %u msg_id: %u \n", schema, msg_id);

            // Handle schema 2 and 3 messages
            if (schema != 2) 
            {
                printf("Can't decode schema: %u\n",schema);
            } 
            else if (msg_id != 2) 
            {
                // Ignore if not id = 2, DECODE
                if(DEBUG) printf("   Ignoring Message\n");
            }
            else
            {
                
                
                /* WSJT-X message structure for schema 2, Decode message                        Id (unique key)        utf8
                                         New                    bool
                                         Time                   QTime
                                         snr                    qint32
                                         Delta time (S)         float (serialized as double)
                                         Delta frequency (Hz)   quint32 
                                         Mode                   utf8
                                         Message                utf8
                                         Low confidence         bool
                */
                
                // Process schema 2 decode message
                // Id Unique Key # bytes
                uint32_t count = 12;  //skipping header bytes
                uint32_t uid_len = 0;
                uint32_t i  = 0;
                double delta_time;

                if(DEBUG) printf("Received WSJT-X message, schema: %u msg_id: %u", schema, msg_id);

        
                // Decode Unique Id it shoulb be WSJT-X
                // Ignore if not WSJT-X 
                uid_len= (buffer[count++] << 24) | (buffer[count++] << 16) | (buffer[count++]<< 8) | (buffer[count++]);
                if(DEBUG) printf("    Unique ID len: %u\n",uid_len);
                char uid[16];
                for(i=0;i< uid_len;i++) uid[i]=buffer[count++];
                uid[count++]='\0';
                if(DEBUG) printf("    Unique ID: %s\n",uid);  // its WSJT-X !

                if(strcmp(uid,"WSJT-X")==0)
                {
                    // decode new bool
                    char  new_bool = buffer[count++];       
                    if(DEBUG) printf("    new_bool %d\n",new_bool);
                    
                    // skip three byte Time vale
                    count += 3; //skipping Time.
    
                    int32_t snr = ((int32_t)buffer[count++] << 24) | ((int32_t)buffer[count++] << 16) | ((int32_t)buffer[count++]<< 8) | ((int32_t)buffer[count++]);
                    if(DEBUG) printf("    snr: %d\n",snr);

                    // decode delta time
                    char dtbuf[8];
                    for (i=0;i<8;i++) dtbuf[i]=buffer[count+i];
                    delta_time = network_buffer_to_double(dtbuf);
                    delta_time_accum(delta_time);
                    count += 8;
                    if(DEBUG) printf("    delta time: %f\n",delta_time);

                    // decode delta frequency - not reliable    
                    uint32_t delta_freq = (buffer[count++] << 24) | (buffer[count++] << 16) | (buffer[count++]<< 8) | (buffer[count++]);
                    if(DEBUG) printf("    delta freq: %u\n",delta_freq);

                    //decode mode
                    char mode = buffer[count++];
                    if(DEBUG) printf("    mode: %d\n",(int)mode);
                }
                else if(DEBUG) printf("Receiving traffic from %s",uid);


            }
        }
    } // end while(1)

    close(sockfd);
    return 0;
}