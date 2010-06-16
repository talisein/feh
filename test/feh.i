#!/usr/bin/env perl
use strict;
use warnings;
use 5.010;

use Test::More tests => 19;
use X11::GUITest qw/
	FindWindowLike
	GetWindowName
	StartApp
	SendKeys
	WaitWindowViewable
/;

my $win;

sub feh_start {
	my ($opts, $files) = @_;
	my $id;

	$opts //= q{};
	$files //= 'test/ok.png';

	StartApp("feh ${opts} ${files}");
	($id) = WaitWindowViewable(qr{^feh});

	if (not $id) {
		BAIL_OUT("Unable to start feh ${opts} ${files}");
	}

	return $id;
}

sub feh_stop {
	SendKeys('{ESC}');
	for (1 .. 10) {
		sleep(0.1);
		if (FindWindowLike(qr{^feh}) == 0) {
			return;
		}
	}
	BAIL_OUT("Unclosed feh window still open, cannot continue");
}

sub test_no_win {
	my ($reason) = @_;

	for (1 .. 10) {
		sleep(0.1);
		if (FindWindowLike(qr{^feh}) == 0) {
			pass("Window closed ($reason)");
			return;
		}
	}
	fail("Window closed ($reason)");
	BAIL_OUT("unclosed window still open, cannot continue");
}

sub test_win_title {
	my ($win, $wtitle) = @_;
	my $rtitle;

	for (1 .. 10) {
		sleep(0.1);
		$rtitle = GetWindowName($win);
		if ($rtitle eq $wtitle) {
			pass("Window has title: $wtitle");
			return;
		}
	}
	fail("Window has title: $wtitle");
	diag("expected: $wtitle");
	diag("     got: $rtitle");
}

if (FindWindowLike(qr{^feh})) {
	BAIL_OUT('It appears you have an open feh window. Please close it.');
}

for my $key (qw/q x {ESC}/) {
	feh_start();
	SendKeys($key);
	test_no_win("$key pressed");
}

$win = feh_start(q{}, 'test/ok.png');
test_win_title($win, 'feh [1 of 1] - test/ok.png');
feh_stop();

$win = feh_start(q{}, 'test/ok.png test/ok.jpg test/ok.gif');
test_win_title($win, 'feh [1 of 3] - test/ok.png');
SendKeys('{RIG}');
test_win_title($win, 'feh [2 of 3] - test/ok.jpg');
SendKeys('n');
test_win_title($win, 'feh [3 of 3] - test/ok.gif');
SendKeys('{SPA}');
test_win_title($win, 'feh [1 of 3] - test/ok.png');
SendKeys('{LEF}');
test_win_title($win, 'feh [3 of 3] - test/ok.gif');
SendKeys('p');
test_win_title($win, 'feh [2 of 3] - test/ok.jpg');
SendKeys('{BAC}');
test_win_title($win, 'feh [1 of 3] - test/ok.png');
SendKeys('p');
test_win_title($win, 'feh [3 of 3] - test/ok.gif');
SendKeys('{DEL}');
test_win_title($win, 'feh [1 of 2] - test/ok.png');
SendKeys('{DEL}');
test_win_title($win, 'feh [1 of 1] - test/ok.jpg');
SendKeys('{DEL}');
test_no_win("Removed all images from slideshow");

feh_start('--cycle-once', 'test/ok.png test/ok.jpg');
for (1 .. 2) {
	SendKeys('{RIG}');
}
test_no_win("--cycle-once -> window closed");

feh_start('--cycle-once --slideshow-delay 0.5',
	'test/ok.png test/ok.jpg test/ok.gif');
sleep(1);
test_no_win('cycle-once + slideshow-delay -> window closed');

$win = feh_start('--cycle-once --slideshow-delay -0.01',
	'test/ok.png test/ok.jpg test/ok.gif');
sleep(0.1);
test_win_title($win, 'feh [1 of 3] - test/ok.png');
SendKeys('h');
test_no_win('cycle-once + negative delay + [h]');