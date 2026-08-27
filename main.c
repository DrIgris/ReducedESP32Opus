#include <_regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <opus/opusfile.h>
#include <string.h>
#include <regex.h>
#include <sys/_types/_null.h>

//constants
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define MAX_FRAME_SIZE 6*960

#define PREFIX_MAX 256 //No prefix for the metadata should be larger than this

float pcm[MAX_FRAME_SIZE*CHANNELS];

typedef struct {
    char* filename;
    char* artist;
    char* album;
    char* title;
} Song;

//LARGE DATA STRUCTURE HOLDING ALL SONGS WILL NEED TO BE LOADED ONTO PSRAM

void split_at_equal(char* prefix, char* str) {
    char* ptr = strchr(str, '=');
    int end = ptr-str;
    strncpy(prefix, str, end);
    prefix[end] = '\0';
}


int main(void) {
    FILE* f = fopen("out.pcm", "wb");

    regex_t reg_artist;
    regex_t reg_album;
    regex_t reg_title;

    regcomp(&reg_artist, "artist", REG_ICASE);
    regcomp(&reg_album, "album", REG_ICASE);
    regcomp(&reg_title, "title", REG_ICASE);

    Song s;

    int samples_read = 0;
    int error = 0;
    OggOpusFile* sng = op_open_file("ByTheLagoon.opus", &error);
    if(error != 0)
        return -1;

    OpusTags* tag_info = op_tags(sng, -1);
    int amt = tag_info->comments;
    for(int i = 0; i < amt; i++) {
        char* prefix = malloc(PREFIX_MAX);
        split_at_equal(prefix, tag_info->user_comments[i]);
        if(regexec(&reg_artist, prefix, 0, NULL, 0) == 0) {
            char* content = tag_info->user_comments[i] + strlen(prefix)+1;
            s.artist = malloc(strlen(content) + 1);
            if(s.artist == NULL)
                return 1;
            strcpy(s.artist, content);
        }
        else if(regexec(&reg_album, prefix, 0, NULL, 0) == 0) {
            char* content = tag_info->user_comments[i] + strlen(prefix)+1;
            s.album = malloc(strlen(content) + 1);
            if(s.album == NULL)
                return 1;
            strcpy(s.album, content);
        }
        else if(regexec(&reg_title, prefix, 0, NULL, 0) == 0) {
            char* content = tag_info->user_comments[i] + strlen(prefix)+1;
            s.title = malloc(strlen(content) + 1);
            if(s.title == NULL)
                return 1;
            strcpy(s.title, content);
        }
        free(prefix);
    }

    // printf("Song Data::\nArtist: %s\nTitle: %s\nAlbum: %s\n", s.artist, s.title, s.album);
    // while ((samples_read = op_read_float_stereo(sng, pcm, MAX_FRAME_SIZE*CHANNELS)) > 0) {
    //     fwrite(pcm, sizeof(float), samples_read*2, f);
    // }


    op_free(sng);

    return 0;
}

