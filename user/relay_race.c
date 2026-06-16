// relay_race.c — Task 2: Relay Race Tournament
//
// Simulates a relay race using the Israeli lock from Task 1.
//
// Layout:
//   NTEAMS teams, each with RUNNERS child processes.
//   One shared Israeli lock represents the baton.
//   Each runner repeatedly acquires the baton, increments its team's
//   score, prints a line, releases the baton, then sleeps briefly.
//   The race ends when any team reaches TARGET_SCORE.
//
// Usage:
//   $ relay_race [favoritism]
//
// favoritism is 0-100 (default 50).  Suggested values:
//   0   — pure FIFO; all teams score roughly equally
//   50  — moderate favouritism; same-team streaks appear
//   100 — maximum favouritism; one team tends to dominate

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NTEAMS        3
#define RUNNERS       5     // runners per team
#define TARGET_SCORE  30

int
main(int argc, char *argv[])
{
  int favoritism = 50;      // default
  if (argc >= 2)
    favoritism = atoi(argv[1]);
  if (favoritism < 0 || favoritism > 100) {
    printf("usage: relay_race [0-100]\n");
    exit(1);
  }

  int lock_id = israeli_create(favoritism);
  if (lock_id < 0) {
    printf("relay_race: israeli_create failed\n");
    exit(1);
  }

  printf("=== Relay Race (favoritism=%d, teams=%d, runners/team=%d, target=%d) ===\n",
         favoritism, NTEAMS, RUNNERS, TARGET_SCORE);

  // Fork all runners.
  for (int team = 0; team < NTEAMS; team++) {
    for (int r = 0; r < RUNNERS; r++) {
      int pid = fork();
      if (pid < 0) {
        printf("relay_race: fork failed\n");
        exit(1);
      }
      if (pid == 0) {
        // ---- child: runner for `team` ----
        setgid(team);

        while (1) {
          if (israeli_acquire(lock_id) < 0)
            exit(0);

          // Check whether the race is already over.
          int done = 0;
          for (int t = 0; t < NTEAMS; t++) {
            if (israeli_score_get(lock_id, t) >= TARGET_SCORE) {
              done = 1;
              break;
            }
          }
          if (done) {
            israeli_release(lock_id);
            exit(0);
          }

          // Increment this team's score.
          int score = israeli_score_inc(lock_id, team);
          printf("Runner %d (Team %d) acquired the baton\nTeam %d score = %d\n",
                 getpid(), team, team, score);

          int winner = (score >= TARGET_SCORE);
          israeli_release(lock_id);

          if (winner) {
            printf("\n*** Team %d wins the race! ***\n\n", team);
            exit(0);
          }

          sleep(1);
        }
        // unreachable
        exit(0);
      }
    }
  }

  // Parent: wait for all NTEAMS * RUNNERS children.
  for (int i = 0; i < NTEAMS * RUNNERS; i++)
    wait(0);

  // Print final scoreboard.
  printf("--- Final Scores ---\n");
  for (int t = 0; t < NTEAMS; t++)
    printf("  Team %d: %d\n", t, israeli_score_get(lock_id, t));

  israeli_destroy(lock_id);
  exit(0);
}
