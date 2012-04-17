#include "iniparser.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    for(int i=1;i<argc;i++) {
        const char *inifilename = argv[i];
        const char *sections[] = { "2grams", "3grams", "4grams", "5grams" };
        const char *keys[] = { "location", "bins", "prefix_length" };

        dictionary *ini = iniparser_load( inifilename );
        
        for(int j=0;j<4;j++) for(int k=0;k<3;k++) {
            char key[1024];
            sprintf( key, "%s:%s", sections[j], key[k] );
            if( k == 0 ) {
                char *rv = iniparser_getstring( ini, key, 0 );
                printf( "%s\t%s\n", key, (rv) ? rv : "<undefined>" );
            } else {
                int rv = iniparser_getint( ini, key, -1 );
                printf( "%s\t%d\n", key, rv );
            }
        }

        iniparser_freedict( ini );
    }
    return 0;
}
