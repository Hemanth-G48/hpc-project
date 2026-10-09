# Work-Dealing

**Work-Dealing** is the mechanism in a task scheduler responsible for deciding where to place a newly created task. 

It is distinct from [[Work Stealing]], which occurs *after* tasks are placed, when an idle thread attempts to acquire tasks from other queues.

## Contrast
* **Work-Dealing:** "Where should this task go when it is first created?"
* **Work-Stealing:** "Which task should an idle thread take?"

In a [[Locality-Aware Task Scheduling|Locality-Aware Task Scheduler]], work-dealing plays a massive role. Instead of arbitrarily tossing a new task into the creator's queue, the work-dealing algorithm calculates the task's expected access cost across all architectural locations based on its [[Task Data Footprint]], and places it in the task queue that offers the best data locality.

## See Also
- [[Work Stealing]]
- [[Locality-Aware Task Scheduling]]
