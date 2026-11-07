using System.Diagnostics;

namespace Lab4_FuturesAndContinuations.Download;

/// <summary>Timestamped, thread-tagged log lines, so the interleaving of the downloads is visible.</summary>
public static class Logger
{
    private static readonly Stopwatch Clock = Stopwatch.StartNew();
    private static readonly object Lock = new();

    /// <summary>Restarts the timestamp clock, so each mode's log starts at 0 ms.</summary>
    public static void Restart()
    {
        lock (Lock)
        {
            Clock.Restart();
        }
    }

    public static void Log(int jobId, string message)
    {
        lock (Lock)
        {
            Console.WriteLine($"[{Clock.ElapsedMilliseconds,6} ms] [thread {Environment.CurrentManagedThreadId,2}] #{jobId} {message}");
        }
    }
}
