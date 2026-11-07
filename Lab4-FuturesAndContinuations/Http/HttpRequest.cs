using System.Text;

namespace Lab4_FuturesAndContinuations.Http;

public static class HttpRequest
{
    /// <summary>
    /// Builds a GET request. HTTP/1.0 + "Connection: close" means the server will not use
    /// chunked transfer encoding and closes the socket after the response.
    /// </summary>
    public static byte[] BuildGet(Uri uri)
    {
        string request =
            $"GET {uri.PathAndQuery} HTTP/1.0\r\n" +
            $"Host: {uri.Host}\r\n" +
            "User-Agent: PDP-Lab4\r\n" +
            "Accept: */*\r\n" +
            "Connection: close\r\n" +
            "\r\n";
        return Encoding.ASCII.GetBytes(request);
    }
}
