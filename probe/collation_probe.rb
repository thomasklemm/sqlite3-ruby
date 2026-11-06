require "sqlite3"
require "objspace"

# Repeats the body of test_replacing_a_collation_releases_the_previous_comparator
# at varying stack depths and, when a stale comparator survives GC, reports
# which GC root category holds it.
MODE = ARGV[0] || "same_thread"
ITER = Integer(ARGV[1] || 2000)

failures = 0
roots = Hash.new(0)

def recurse(n, &blk) = n.zero? ? blk.call : recurse(n - 1, &blk)

ITER.times do |i|
  klass = Class.new { def compare(a, b) = a <=> b }
  db = SQLite3::Database.new(":memory:")
  recurse(i % 50) do
    if MODE == "other_thread"
      Thread.new { 3.times { db.collation "foo", klass.new } }.join
    else
      3.times { db.collation "foo", klass.new }
    end
  end
  GC.start(full_mark: true, immediate_sweep: true)
  live = ObjectSpace.each_object(klass).to_a
  if live.size != 1
    failures += 1
    current = db.collations["foo"]
    stale = live.reject { |o| o.equal?(current) }
    ObjectSpace.reachable_objects_from_root.each do |category, objs|
      stale.each { |s| roots[category] += 1 if objs.any? { |o| o.equal?(s) } }
    end
  end
  live = nil
  db.close
end

puts "#{RUBY_DESCRIPTION} mode=#{MODE} iterations=#{ITER} failures=#{failures} stale_roots=#{roots.inspect}"
