package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"os"
	"strings"

	"github.com/metacubex/mihomo/common/convert"
)

func preprocessSubscription(subscription string) string {
	lines := strings.Split(subscription, "\n")
	var result []string

	for _, line := range lines {
		line = strings.TrimRight(line, " \r")
		if line == "" {
			result = append(result, line)
			continue
		}

		if decoded, err := url.QueryUnescape(line); err == nil {
			line = decoded
		}

		result = append(result, line)
	}

	return strings.Join(result, "\n")
}

func main() {
	input, err := io.ReadAll(os.Stdin)
	if err != nil {
		fmt.Fprintf(os.Stderr, `{"error": "failed to read stdin: %s"}`, err.Error())
		os.Exit(1)
	}

	subscription := preprocessSubscription(string(input))

	proxies, err := convert.ConvertsV2Ray([]byte(subscription))
	if err != nil {
		errJSON, _ := json.Marshal(map[string]string{
			"error": err.Error(),
		})
		fmt.Println(string(errJSON))
		os.Exit(1)
	}

	result, err := json.Marshal(proxies)
	if err != nil {
		fmt.Fprintf(os.Stderr, `{"error": "failed to marshal result: %s"}`, err.Error())
		os.Exit(1)
	}

	fmt.Println(string(result))
}
