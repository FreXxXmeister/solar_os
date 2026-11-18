"""The NTP wait must outlast lwIP's next try after lost SNTP or DNS exchanges."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

# ESP-IDF 5.5 defaults (no sdkconfig.defaults file sets them):
# CONFIG_LWIP_SNTP_STARTUP_DELAY=y, CONFIG_LWIP_SNTP_MAXIMUM_STARTUP_DELAY=5000
# delay the first request after sntp_init by a random 0..5000 ms (lwip sntp.c sntp_init).
SNTP_STARTUP_DELAY_MAX_MS = 5000
# lwIP sntp_opts.h: SNTP_RECV_TIMEOUT 15000. After a request, lwIP waits this long
# for the reply, then calls sntp_try_next_server.
SNTP_RECV_TIMEOUT_MS = 15000
# lwIP sntp_opts.h: SNTP_RETRY_TIMEOUT = SNTP_RECV_TIMEOUT. In a build with one
# server slot (CONFIG_LWIP_SNTP_MAX_SERVERS=1), sntp_try_next_server finds no other
# server and calls sntp_retry, which waits this long before the next request. With
# more slots lwIP moves on at once.
SNTP_RETRY_TIMEOUT_MS = 15000
# lwIP dns.c with DNS_MAX_RETRIES 4 and DNS_TMR_INTERVAL 1000: sends at 0, 1, 2 and 4 s,
# and reports a failed lookup at 7 s when only one DNS server is set; sntp_dns_found
# then calls sntp_try_next_server, and so sntp_retry.
DNS_FAIL_MS = 7000
# Time left for the last try's lookup and reply.
REPLY_WINDOW_MS = 5000


def default_timeout_ms() -> int:
    header = (ROOT / "src" / "services" / "solar_os_time.h").read_text(encoding="utf-8")
    match = re.search(r"#define SOLAR_OS_NTP_DEFAULT_TIMEOUT_MS (\d+)U?", header)
    assert match is not None
    return int(match.group(1))


class NtpTimeoutTest(unittest.TestCase):
    def test_default_wait_covers_a_failed_lookup(self) -> None:
        # Full start delay, a lookup that fails, lwIP's retry delay, then the second try.
        second_try_ms = SNTP_STARTUP_DELAY_MAX_MS + DNS_FAIL_MS + SNTP_RETRY_TIMEOUT_MS
        self.assertGreaterEqual(default_timeout_ms(), second_try_ms + REPLY_WINDOW_MS)

    def test_default_wait_covers_a_lost_reply(self) -> None:
        # Full start delay, the receive timeout for the lost reply, lwIP's retry delay,
        # then the second request.
        second_try_ms = SNTP_STARTUP_DELAY_MAX_MS + SNTP_RECV_TIMEOUT_MS + SNTP_RETRY_TIMEOUT_MS
        self.assertGreaterEqual(default_timeout_ms(), second_try_ms + REPLY_WINDOW_MS)

    def test_default_wait_covers_two_silent_servers(self) -> None:
        # Three slots: full start delay, the receive timeout of each of two servers that
        # do not answer, then the third server.
        third_try_ms = SNTP_STARTUP_DELAY_MAX_MS + 2 * SNTP_RECV_TIMEOUT_MS
        self.assertGreaterEqual(default_timeout_ms(), third_try_ms + REPLY_WINDOW_MS)

    def test_manual_states_the_default_wait(self) -> None:
        ms = default_timeout_ms()
        manual = ROOT / "doc" / "manual"
        for page, text in (("commands.md", f"Waits up to {ms // 1000} s"),
                           ("jobs.reference.md", f"| Wait per sync | `{ms // 1000}` seconds"),
                           ("python.time.md", f"`timeout_ms` to `{ms}`"),
                           ("lua.time.md", f"`timeout_ms` to `{ms}`")):
            with self.subTest(page=page):
                self.assertIn(text, (manual / page).read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
