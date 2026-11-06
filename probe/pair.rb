# Runs a forking test directly before test_busy_timeout, N times, and counts
# how often test_busy_timeout fails.
$LOAD_PATH.unshift File.expand_path("test")
require "helper"
require "test_discarding"
require "test_integration_pending"

first = ARGV[0] || "test_fork_does_not_discard_readonly_connections"
n = Integer(ARGV[1] || 20)
fails = []
n.times do
  SQLite3::TestDiscardDatabase.new(first).run
  r = IntegrationPendingTestCase.new("test_busy_timeout").run
  fails << r.failures.first.message.lines.last.strip unless r.passed?
end
puts "#{RUBY_VERSION} #{first}: test_busy_timeout failed #{fails.size}/#{n} #{fails.first(3).inspect}"
