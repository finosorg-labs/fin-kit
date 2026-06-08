module github.com/finosorg-labs/fin-kit/exchange

go 1.26.1

require github.com/finosorg-labs/exchange-c/orderbook v1.0.5

require (
	github.com/finosorg-labs/ds-c/sdk v1.1.4 // indirect
	github.com/finosorg-labs/hash-c/sdk v1.0.1 // indirect
)

replace github.com/finosorg-labs/exchange => ../core/exchange
