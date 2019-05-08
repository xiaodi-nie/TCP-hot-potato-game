#define MAX_HOPS 512
#define MAX_NAME 128

typedef struct potato_t{
  int num_hops; 
  int trace[MAX_HOPS];
  int remain_hops; 
  
} potato;


typedef struct player_t{
  int player_id;
  int right_id;
  int left_id;
  int num_hops;
  int num_players;
  int right_port;
  char right_name[MAX_NAME];
} player;
