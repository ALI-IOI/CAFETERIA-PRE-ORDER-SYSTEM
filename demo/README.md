# Local demo (fake data only)

Runs the server with the **sample** university list (fake domain `uni.example`)
and a fake clock starting at 08:30, so the whole day can be shown in minutes.
Emails are written to `demo/data/outbox/` (open the `.txt` files to read them).

```bash
cmake --build build
printf 'demo-staff-pass\ndemo-staff-pass\n' | build/cafeteria_admin --data demo/data add-staff demo.staff "Demo Staff"
build/cafeteria_server --config demo/config --data demo/data --fake-clock 2026-10-23T08:30 --tick-seconds 0
```

Open http://127.0.0.1:8080 for the landing page; students log in at `/login.html`, staff at `/staff/login.html`.

| Account | Login | Password (test only) |
|---|---|---|
| Staff | `demo.staff` | `demo-staff-pass` |
| Student | register `ayon@uni.example`, ID `1072520005101030` | `demo-student-pass` |
| Teacher | register `teacher1@uni.example`, ID `2000000000000001` | `demo-teacher-pass` |

The verification code is in the newest `*_verification.txt` file in `demo/data/outbox/`.

Move the fake clock (e.g. to show reminders and expiry):

```bash
curl -X POST -H "X-Cafe: 1" -d minutes=60 http://127.0.0.1:8080/api/dev/advance
```

Order history lists earlier days only. To see it, reserve something, then move the clock past
midnight (for example `-d minutes=960` from 08:30) and log in again — sessions end when the clock
jumps that far. The history is under My reservations; My account is in the top bar.
