/* This program reads the text output of `bgpdump' and extracts some fields.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_LINE_LENGTH 1024
#define MAX_PREFIXES_PER_ENTRY 1024

int string_prefix_matches(char* s, char* prefix);

typedef struct prefix_t {
	u_int32_t ip;
	int len;
} prefix_t;

void set_prefix_is_interesting(u_int32_t ip, int len);
int  get_prefix_is_interesting(u_int32_t ip, int len);

int main(int argc, char** argv)
{
	char  line[MAX_LINE_LENGTH];
	
	int total_entries = 0;
	int total_interesting_withdrawals = 0;
	
	/* In each iteration of this loop, we read one dump entry
	 * (i.e., a BGP update message or RIB entry) and pick out some fields.
	 */
	while (!feof(stdin)) {
	    /* Variables representing the fields in one entry. */
		prefix_t prefixes[MAX_PREFIXES_PER_ENTRY];
		int      num_prefixes         = 0;
		int      sec_since_jan27_2011 = -1;
		int      is_announce          = 0;
		int      is_withdraw          = 0;
		char*    as_path              = NULL;
		int      last_asn             = -1;
		int      is_interesting       = 0;
		int      count_this_update    = 0;
		int      i;
		int      len;
		u_int32_t ip, ip1, ip2, ip3, ip4;
		
		/* In each iteration of this loop, we read one line of bgpdump's output.
		 * A blank line has strlen(line) <= 1 and indicates the end of the entry.
		 */
		while (fgets(line, MAX_LINE_LENGTH, stdin) && (strlen(line) > 1)) {
			//printf("LINE: %s", line);

			if (string_prefix_matches(line, "TIME: 01/27/11 ")) {
				assert(strlen(line) >= 23);
				int hour = atoi(line+15);
				int min  = atoi(line+18);
				int sec  = atoi(line+21);
				sec_since_jan27_2011 = 3600*hour + 60*min + sec;
			}
			else if (string_prefix_matches(line, "ANNOUNCE"))
				is_announce = 1;
			else if (string_prefix_matches(line, "WITHDRAW"))
				is_withdraw = 1;
			else if (string_prefix_matches(line, "ASPATH: ")) {
				/* Here the line looks something like "ASPATH: 5413 1299 3257 18747 11014". */
				char* last_space = strrchr(line, ' ');
				int egypt_as[5] = {5536, 8452, 24835, 24863, 36992};
				char* end;
				long first_as = strtol(line+8,&end,10);
				//printf("first as = %ld\n",first_as);
				if (last_space && sscanf(last_space, " %d\n", &last_asn) == 1) {
					for(int i=0;i<5;i++){
						if(last_asn == egypt_as[i]){
							is_interesting = 1;
							//printf("interesting found\n");
						}				
					}
					/* `last_asn' is now an integer containing the last ASN in the AS PATH.
					 * You might find this useful, but for now, we don't do
					 * anything with it. */
				}
			}
			else if (    (sscanf(line,       "  %u.%u.%u.%u/%d\n", &ip1, &ip2, &ip3, &ip4, &len) == 5)
			          || (sscanf(line, "PREFIX: %u.%u.%u.%u/%d\n", &ip1, &ip2, &ip3, &ip4, &len) == 5)) {
				/* Found a prefix associated with this update */
				ip = (ip1 << 24) | (ip2 << 16) | (ip3 << 8) | ip4;
				if (num_prefixes < MAX_PREFIXES_PER_ENTRY) {
					prefixes[num_prefixes].ip  = ip;
					prefixes[num_prefixes].len = len;
					num_prefixes++;
				}
				else
					fprintf(stderr, "Warning: number of prefixes in one BGP update exceeded my maximum of %d; ignoring this update\n", MAX_PREFIXES_PER_ENTRY);
			}
		}
		/* At this point, we are done reading the RIB entry or update message. */

		/* If we thought this entry was "interesting", then we remember any
		 * IP prefixes associated with it. */
		if (is_interesting) {
			for (i = 0; i < num_prefixes; i++)
				set_prefix_is_interesting(prefixes[i].ip, prefixes[i].len);
		}

		/* Let's count any entry which withdraws "interesting" prefixes. */
		if (is_withdraw) {
			for (i = 0; i < num_prefixes; i++)
				if (get_prefix_is_interesting(prefixes[i].ip, prefixes[i].len))
					count_this_update = 1;
		}
		/* Now print out some info about it. */
		if (count_this_update && sec_since_jan27_2011 >= 0) {
			total_interesting_withdrawals += num_prefixes;
			/* Print out the current time (in sec since beginning of 1/27/2011), and the total
			 * number of "interesting" prefix-updates we have seen so far. */
			printf("%d %d\n", sec_since_jan27_2011, total_interesting_withdrawals);
		}
		total_entries++;
	}
	//printf("Processed %d entires in total.\n", total_entries);
}

int string_prefix_matches(char* s, char* prefix)
{
	return !strncmp(s, prefix, strlen(prefix));
}



#define MAX_INTERESTING 10000

prefix_t interesting[MAX_INTERESTING];
int num_interesting = 0;

void set_prefix_is_interesting(u_int32_t ip, int len) {
	if (!get_prefix_is_interesting(ip,len)) {
		assert(num_interesting < MAX_INTERESTING);
		interesting[num_interesting].ip = ip;
		interesting[num_interesting].len = len;
		num_interesting++;
		//printf("%d are now interesting\n", num_interesting);
	}
}

int  get_prefix_is_interesting(u_int32_t ip, int len) {
	int i;
	for (i = 0; i < num_interesting; i++) {
		if (interesting[i].ip == ip && interesting[i].len == len)
			return 1;
	}
	return 0;
}
