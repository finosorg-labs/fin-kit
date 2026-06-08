module github.com/finosorg-labs/fin-kit/hft

go 1.26.1

require (
	github.com/finosorg-labs/exchange-c/orderbook v1.0.5
	github.com/finosorg-labs/platform v1.0.5
)

require (
	github.com/finosorg-labs/ds-c/sdk v1.1.4 // indirect
	github.com/finosorg-labs/hash-c/sdk v1.0.1 // indirect
)

replace github.com/finosorg-labs/exchange => ../core/exchange

replace github.com/finosorg-labs/platform => ../core/platform
