using Lab4_FuturesAndContinuations.Download;

// Usage: Lab4-FuturesAndContinuations [all|callbacks|continuewith|async] [url1 url2 ...]
// With no mode (or "all"), the three implementations run one after another.
// With no URLs, a default set of plain-HTTP URLs is downloaded.

string[] defaultUrls =
{
    "http://info.cern.ch/",
    "http://info.cern.ch/hypertext/WWW/TheProject.html",
    "http://example.com/",
    "http://httpbin.org/bytes/50000",
    "http://textfiles.com/100/",
};
string[] allModes = { "callbacks", "continuewith", "async" };

string mode = args.Length > 0 ? args[0] : "all";
if (mode != "all" && !allModes.Contains(mode))
{
    Console.WriteLine("Usage: Lab4-FuturesAndContinuations [all|callbacks|continuewith|async] [url ...]");
    return 1;
}

string[] urls = args.Length > 1 ? args[1..] : defaultUrls;
string[] modesToRun = mode == "all" ? allModes : new[] { mode };

bool anyFailed = false;
foreach (string m in modesToRun)
{
    anyFailed |= RunMode(m, urls);
}
return anyFailed ? 2 : 0;

// Runs one implementation over all URLs. Returns true if any download failed.
static bool RunMode(string mode, string[] urls)
{
    // fresh jobs (sockets, parsers) for every mode
    var jobs = new List<DownloadJob>();
    for (int i = 0; i < urls.Length; i++)
    {
        var uri = new Uri(urls[i]);
        if (uri.Scheme != Uri.UriSchemeHttp)
        {
            Console.WriteLine($"Skipping {uri}: only plain http:// is supported (no TLS)");
            continue;
        }
        jobs.Add(new DownloadJob(i + 1, uri));
    }

    string outputDir = Path.Combine("downloads", mode);
    Directory.CreateDirectory(outputDir);

    Console.WriteLine();
    Console.WriteLine($"################ Mode: {mode} ################");
    Console.WriteLine($"Downloading {jobs.Count} files into {Path.GetFullPath(outputDir)}");
    Console.WriteLine();
    Logger.Restart();

    switch (mode)
    {
        case "callbacks":
            CallbackDownloader.Run(jobs, outputDir);
            break;
        case "continuewith":
            ContinueWithDownloader.Run(jobs, outputDir);
            break;
        case "async":
            AsyncAwaitDownloader.Run(jobs, outputDir);
            break;
    }

    Report.PrintSummary(jobs);
    return jobs.Any(j => j.Error != null);
}
